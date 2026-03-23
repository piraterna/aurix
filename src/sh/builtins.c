#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#include "builtins.h"

extern char **environ;

static int builtin_cd(int argc, char *argv[], bool *should_exit);
static int builtin_pwd(int argc, char *argv[], bool *should_exit);
static int builtin_export(int argc, char *argv[], bool *should_exit);
static int builtin_unset(int argc, char *argv[], bool *should_exit);
static int builtin_ls(int argc, char *argv[], bool *should_exit);
static int builtin_cat(int argc, char *argv[], bool *should_exit);
static int builtin_echo(int argc, char *argv[], bool *should_exit);
static int builtin_help(int argc, char *argv[], bool *should_exit);
static int builtin_clear(int argc, char *argv[], bool *should_exit);
static int builtin_env(int argc, char *argv[], bool *should_exit);
static int builtin_exit(int argc, char *argv[], bool *should_exit);
static int builtin_mkdir(int argc, char *argv[], bool *should_exit);
static int builtin_rmdir(int argc, char *argv[], bool *should_exit);
static int builtin_rm(int argc, char *argv[], bool *should_exit);
static int builtin_mv(int argc, char *argv[], bool *should_exit);
static int builtin_cp(int argc, char *argv[], bool *should_exit);
static int builtin_ln(int argc, char *argv[], bool *should_exit);
static int builtin_stat(int argc, char *argv[], bool *should_exit);
static int builtin_chmod(int argc, char *argv[], bool *should_exit);
static int builtin_touch(int argc, char *argv[], bool *should_exit);
static int builtin_sleep(int argc, char *argv[], bool *should_exit);
static int builtin_kill(int argc, char *argv[], bool *should_exit);
static int builtin_umask(int argc, char *argv[], bool *should_exit);

#define BUILTIN_LIST          \
	X(cd, builtin_cd)         \
	X(pwd, builtin_pwd)       \
	X(export, builtin_export) \
	X(unset, builtin_unset)   \
	X(ls, builtin_ls)         \
	X(cat, builtin_cat)       \
	X(echo, builtin_echo)     \
	X(help, builtin_help)     \
	X(clear, builtin_clear)   \
	X(env, builtin_env)       \
	X(exit, builtin_exit)     \
	X(mkdir, builtin_mkdir)   \
	X(rmdir, builtin_rmdir)   \
	X(rm, builtin_rm)         \
	X(mv, builtin_mv)         \
	X(cp, builtin_cp)         \
	X(ln, builtin_ln)         \
	X(stat, builtin_stat)     \
	X(chmod, builtin_chmod)   \
	X(touch, builtin_touch)   \
	X(sleep, builtin_sleep)   \
	X(kill, builtin_kill)     \
	X(umask, builtin_umask)

static const struct builtin builtin_table[] = {
#define X(name, fn) { #name, fn },
	BUILTIN_LIST
#undef X
};

static const size_t builtin_table_count =
	sizeof(builtin_table) / sizeof(builtin_table[0]);

size_t builtin_count(void)
{
	return builtin_table_count;
}

const struct builtin *builtin_lookup(const char *name)
{
	if (!name)
		return NULL;

	for (size_t i = 0; i < builtin_table_count; i++) {
		if (strcmp(name, builtin_table[i].name) == 0)
			return &builtin_table[i];
	}

	return NULL;
}

static int builtin_cd(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;
	const char *path = NULL;

	if (argc < 2) {
		path = getenv("HOME");
		if (path == NULL) {
			fprintf(stderr, "cd: HOME not set\n");
			return 1;
		}
	} else {
		path = argv[1];
	}

	if (chdir(path) != 0) {
		fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
		return 1;
	}

	return 0;
}

static int builtin_pwd(int argc, char *argv[], bool *should_exit)
{
	(void)argc;
	(void)argv;
	(void)should_exit;

	char cwd[PATH_MAX];

	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		fprintf(stderr, "pwd: %s\n", strerror(errno));
		return 1;
	}

	printf("%s\n", cwd);
	return 0;
}

static int builtin_export(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 2) {
		fprintf(stderr, "export: usage: export NAME=VALUE\n");
		return 1;
	}

	char *eq = strchr(argv[1], '=');
	if (eq == NULL) {
		fprintf(stderr, "export: expected NAME=VALUE\n");
		return 1;
	}

	*eq = '\0';
	const char *name = argv[1];
	const char *value = eq + 1;

	if (name[0] == '\0') {
		fprintf(stderr, "export: empty variable name\n");
		return 1;
	}

	if (setenv(name, value, 1) != 0) {
		fprintf(stderr, "export: %s\n", strerror(errno));
		return 1;
	}

	return 0;
}

static char *expand_echo_escapes(const char *input)
{
	if (!input)
		return NULL;

	size_t len = strlen(input);
	char *out = malloc(len + 1);
	if (!out)
		return NULL;

	size_t out_len = 0;
	for (size_t i = 0; i < len; i++) {
		if (input[i] != '\\') {
			out[out_len++] = input[i];
			continue;
		}

		if (i + 1 >= len) {
			out[out_len++] = '\\';
			break;
		}

		char c = input[++i];
		switch (c) {
		case 'e':
			out[out_len++] = '\033';
			break;
		case 'n':
			out[out_len++] = '\n';
			break;
		case 't':
			out[out_len++] = '\t';
			break;
		case 'r':
			out[out_len++] = '\r';
			break;
		case 'a':
			out[out_len++] = '\a';
			break;
		case 'b':
			out[out_len++] = '\b';
			break;
		case 'f':
			out[out_len++] = '\f';
			break;
		case 'v':
			out[out_len++] = '\v';
			break;
		case '\\':
			out[out_len++] = '\\';
			break;
		case 'x': {
			int value = 0;
			int digits = 0;
			while (i + 1 < len && digits < 2 &&
				   isxdigit((unsigned char)input[i + 1])) {
				char h = input[i + 1];
				value *= 16;
				if (h >= '0' && h <= '9')
					value += h - '0';
				else if (h >= 'a' && h <= 'f')
					value += 10 + (h - 'a');
				else if (h >= 'A' && h <= 'F')
					value += 10 + (h - 'A');
				i++;
				digits++;
			}
			if (digits > 0)
				out[out_len++] = (char)value;
			else
				out[out_len++] = 'x';
			break;
		}
		case '0': {
			int value = 0;
			int digits = 0;
			while (i + 1 < len && digits < 3) {
				char o = input[i + 1];
				if (o < '0' || o > '7')
					break;
				value = value * 8 + (o - '0');
				i++;
				digits++;
			}
			out[out_len++] = (char)value;
			break;
		}
		default:
			out[out_len++] = c;
			break;
		}
	}

	out[out_len] = '\0';
	return out;
}

static int builtin_echo(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;
	bool enable_escapes = false;
	int start = 1;
	if (argc > 1 && strcmp(argv[1], "-e") == 0) {
		enable_escapes = true;
		start = 2;
	}

	for (int i = start; i < argc; i++) {
		char *escaped = NULL;
		const char *out = argv[i];
		if (enable_escapes) {
			escaped = expand_echo_escapes(argv[i]);
			if (escaped)
				out = escaped;
		}
		if (i > start)
			putchar(' ');
		fputs(out, stdout);
		free(escaped);
	}
	putchar('\n');
	return 0;
}

static int builtin_cat(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	int status = 0;
	char buf[4096];
	bool wrote_any = false;
	char last_char = '\n';

	int start = 1;
	int count = argc - 1;
	if (argc < 2) {
		start = 0;
		count = 1;
	}

	for (int i = 0; i < count; i++) {
		int fd = STDIN_FILENO;
		if (start > 0) {
			fd = open(argv[start + i], O_RDONLY, 0);
			if (fd < 0) {
				fprintf(stderr, "cat: %s: %s\n", argv[start + i],
						strerror(errno));
				status = 1;
				continue;
			}
		}

		for (;;) {
			ssize_t n = read(fd, buf, sizeof(buf));
			if (n == 0)
				break;
			if (n < 0) {
				if (start > 0)
					fprintf(stderr, "cat: %s: %s\n", argv[start + i],
							strerror(errno));
				else
					fprintf(stderr, "cat: stdin: %s\n", strerror(errno));
				status = 1;
				break;
			}
			ssize_t off = 0;
			while (off < n) {
				ssize_t wrote =
					write(STDOUT_FILENO, buf + off, (size_t)(n - off));
				if (wrote < 0) {
					fprintf(stderr, "cat: write: %s\n", strerror(errno));
					status = 1;
					break;
				}
				wrote_any = true;
				last_char = buf[off + wrote - 1];
				off += wrote;
			}
			if (off < n)
				break;
		}

		if (start > 0)
			close(fd);
	}

	if (wrote_any && last_char != '\n')
		fputs("%\n", stdout);

	return status;
}

static void ls_print_entry(const char *name)
{
	if (!name || name[0] == '\0')
		return;
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return;
	printf("%s  ", name);
}

static int builtin_ls(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	int status = 0;
	int first = 1;
	int count = argc < 2 ? 1 : argc - 1;

	for (int idx = 0; idx < count; idx++) {
		const char *path = (argc < 2) ? "." : argv[idx + 1];
		DIR *dir = opendir(path);
		if (!dir) {
			struct stat st;
			if (stat(path, &st) == 0) {
				printf("%s\n", path);
				continue;
			}
			fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
			status = 1;
			continue;
		}

		if (!first || count > 1)
			printf("%s:\n", path);
		first = 0;

		struct dirent *ent;
		while ((ent = readdir(dir)) != NULL) {
			ls_print_entry(ent->d_name);
		}
		printf("\n");
		closedir(dir);
	}

	return status;
}

static int builtin_unset(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 2) {
		fprintf(stderr, "unset: usage: unset NAME\n");
		return 1;
	}

	if (unsetenv(argv[1]) != 0) {
		fprintf(stderr, "unset: %s\n", strerror(errno));
		return 1;
	}

	return 0;
}

static int builtin_help(int argc, char *argv[], bool *should_exit)
{
	(void)argc;
	(void)argv;
	(void)should_exit;

	fputs("builtins:", stdout);
	for (size_t i = 0; i < builtin_table_count; i++) {
		fputc(' ', stdout);
		fputs(builtin_table[i].name, stdout);
	}
	putchar('\n');
	return 0;
}

static int builtin_env(int argc, char *argv[], bool *should_exit)
{
	(void)argc;
	(void)argv;
	(void)should_exit;

	if (!environ)
		return 0;
	for (char **entry = environ; *entry; entry++) {
		puts(*entry);
	}
	return 0;
}

static int builtin_clear(int argc, char *argv[], bool *should_exit)
{
	(void)argc;
	(void)argv;
	(void)should_exit;

	fputs("\033[2J\033[H", stdout);
	fflush(stdout);
	return 0;
}

static int builtin_exit(int argc, char *argv[], bool *should_exit)
{
	if (should_exit)
		*should_exit = true;

	if (argc >= 2)
		return atoi(argv[1]);
	return 0;
}

static int builtin_mkdir(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 2) {
		fprintf(stderr, "mkdir: usage: mkdir PATH...\n");
		return 1;
	}

	int status = 0;
	for (int i = 1; i < argc; i++) {
		if (mkdir(argv[i], 0777) != 0) {
			fprintf(stderr, "mkdir: %s: %s\n", argv[i], strerror(errno));
			status = 1;
		}
	}

	return status;
}

static int builtin_rmdir(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 2) {
		fprintf(stderr, "rmdir: usage: rmdir PATH...\n");
		return 1;
	}

	int status = 0;
	for (int i = 1; i < argc; i++) {
		if (rmdir(argv[i]) != 0) {
			fprintf(stderr, "rmdir: %s: %s\n", argv[i], strerror(errno));
			status = 1;
		}
	}

	return status;
}

static int builtin_rm(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 2) {
		fprintf(stderr, "rm: usage: rm PATH...\n");
		return 1;
	}

	int status = 0;
	for (int i = 1; i < argc; i++) {
		if (unlink(argv[i]) != 0) {
			fprintf(stderr, "rm: %s: %s\n", argv[i], strerror(errno));
			status = 1;
		}
	}

	return status;
}

static int builtin_mv(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc != 3) {
		fprintf(stderr, "mv: usage: mv SRC DST\n");
		return 1;
	}

	if (rename(argv[1], argv[2]) != 0) {
		fprintf(stderr, "mv: %s -> %s: %s\n", argv[1], argv[2],
				strerror(errno));
		return 1;
	}

	return 0;
}

static int copy_file(const char *src, const char *dst)
{
	int in_fd = open(src, O_RDONLY);
	if (in_fd < 0) {
		fprintf(stderr, "cp: %s: %s\n", src, strerror(errno));
		return 1;
	}

	struct stat st;
	mode_t mode = 0666;
	if (fstat(in_fd, &st) == 0)
		mode = st.st_mode & 0777;

	int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (out_fd < 0) {
		fprintf(stderr, "cp: %s: %s\n", dst, strerror(errno));
		close(in_fd);
		return 1;
	}

	char buf[4096];
	int status = 0;
	for (;;) {
		ssize_t n = read(in_fd, buf, sizeof(buf));
		if (n == 0)
			break;
		if (n < 0) {
			fprintf(stderr, "cp: %s: %s\n", src, strerror(errno));
			status = 1;
			break;
		}
		ssize_t off = 0;
		while (off < n) {
			ssize_t wrote = write(out_fd, buf + off, (size_t)(n - off));
			if (wrote < 0) {
				fprintf(stderr, "cp: %s: %s\n", dst, strerror(errno));
				status = 1;
				break;
			}
			off += wrote;
		}
		if (off < n)
			break;
	}

	close(in_fd);
	close(out_fd);
	return status;
}

static int builtin_cp(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc != 3) {
		fprintf(stderr, "cp: usage: cp SRC DST\n");
		return 1;
	}

	return copy_file(argv[1], argv[2]);
}

static int builtin_ln(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	bool use_symlink = false;
	int argi = 1;
	if (argc > 1 && strcmp(argv[1], "-s") == 0) {
		use_symlink = true;
		argi = 2;
	}

	if (argc - argi != 2) {
		fprintf(stderr, "ln: usage: ln [-s] SRC DST\n");
		return 1;
	}

	const char *src = argv[argi];
	const char *dst = argv[argi + 1];
	int rc = use_symlink ? symlink(src, dst) : link(src, dst);
	if (rc != 0) {
		fprintf(stderr, "ln: %s -> %s: %s\n", src, dst, strerror(errno));
		return 1;
	}

	return 0;
}

static const char *stat_type_name(mode_t mode)
{
	if (S_ISREG(mode))
		return "file";
	if (S_ISDIR(mode))
		return "dir";
	if (S_ISLNK(mode))
		return "link";
	if (S_ISCHR(mode))
		return "char";
	if (S_ISBLK(mode))
		return "block";
	if (S_ISFIFO(mode))
		return "fifo";
	if (S_ISSOCK(mode))
		return "sock";
	return "unknown";
}

static int builtin_stat(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 2) {
		fprintf(stderr, "stat: usage: stat PATH...\n");
		return 1;
	}

	int status = 0;
	for (int i = 1; i < argc; i++) {
		struct stat st;
		if (stat(argv[i], &st) != 0) {
			fprintf(stderr, "stat: %s: %s\n", argv[i], strerror(errno));
			status = 1;
			continue;
		}
		printf("%s: type=%s mode=%o size=%lld\n", argv[i],
			   stat_type_name(st.st_mode), (unsigned)(st.st_mode & 0777),
			   (long long)st.st_size);
	}

	return status;
}

static int builtin_chmod(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 3) {
		fprintf(stderr, "chmod: usage: chmod MODE PATH...\n");
		return 1;
	}

	char *end = NULL;
	long mode = strtol(argv[1], &end, 8);
	if (!argv[1][0] || (end && *end != '\0') || mode < 0 || mode > 07777) {
		fprintf(stderr, "chmod: invalid mode: %s\n", argv[1]);
		return 1;
	}

	int status = 0;
	for (int i = 2; i < argc; i++) {
		if (chmod(argv[i], (mode_t)mode) != 0) {
			fprintf(stderr, "chmod: %s: %s\n", argv[i], strerror(errno));
			status = 1;
		}
	}

	return status;
}

static int builtin_touch(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 2) {
		fprintf(stderr, "touch: usage: touch PATH...\n");
		return 1;
	}

	int status = 0;
	for (int i = 1; i < argc; i++) {
		int fd = open(argv[i], O_WRONLY | O_CREAT, 0666);
		if (fd < 0) {
			fprintf(stderr, "touch: %s: %s\n", argv[i], strerror(errno));
			status = 1;
			continue;
		}
		close(fd);
	}

	return status;
}

static int builtin_sleep(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc != 2) {
		fprintf(stderr, "sleep: usage: sleep SECONDS\n");
		return 1;
	}

	char *end = NULL;
	long seconds = strtol(argv[1], &end, 10);
	if (!argv[1][0] || (end && *end != '\0') || seconds < 0) {
		fprintf(stderr, "sleep: invalid seconds: %s\n", argv[1]);
		return 1;
	}

	while (seconds > 0)
		seconds = sleep((unsigned int)seconds);
	return 0;
}

static int builtin_kill(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "kill: usage: kill PID [SIG]\n");
		return 1;
	}

	char *end = NULL;
	long pid = strtol(argv[1], &end, 10);
	if (!argv[1][0] || (end && *end != '\0') || pid <= 0) {
		fprintf(stderr, "kill: invalid pid: %s\n", argv[1]);
		return 1;
	}

	int sig = 15;
	if (argc == 3) {
		end = NULL;
		long parsed = strtol(argv[2], &end, 10);
		if (!argv[2][0] || (end && *end != '\0') || parsed < 0 ||
			parsed > 255) {
			fprintf(stderr, "kill: invalid signal: %s\n", argv[2]);
			return 1;
		}
		sig = (int)parsed;
	}

	if (kill((pid_t)pid, sig) != 0) {
		fprintf(stderr, "kill: %ld: %s\n", pid, strerror(errno));
		return 1;
	}

	return 0;
}

static int builtin_umask(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc == 1) {
		mode_t current = umask(0);
		umask(current);
		printf("%03o\n", (unsigned)(current & 0777));
		return 0;
	}

	if (argc != 2) {
		fprintf(stderr, "umask: usage: umask [MODE]\n");
		return 1;
	}

	char *end = NULL;
	long mode = strtol(argv[1], &end, 8);
	if (!argv[1][0] || (end && *end != '\0') || mode < 0 || mode > 0777) {
		fprintf(stderr, "umask: invalid mode: %s\n", argv[1]);
		return 1;
	}

	umask((mode_t)mode);
	return 0;
}
