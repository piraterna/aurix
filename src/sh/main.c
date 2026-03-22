#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 128

extern char **environ;

static int exec_with_path(const char *file, char *argv[])
{
	if (!file || !*file) {
		errno = ENOENT;
		return -1;
	}

	if (strchr(file, '/')) {
		execve(file, argv, environ);
		return -1;
	}

	const char *path_env = getenv("PATH");
	int last_errno = 0;
	const char *segment = path_env;
	while (segment) {
		const char *next = strchr(segment, ':');
		size_t seg_len = next ? (size_t)(next - segment) : strlen(segment);
		const char *dir = segment;
		char candidate[PATH_MAX];

		if (seg_len == 0) {
			dir = ".";
			seg_len = 1;
		}

		int written = snprintf(candidate, sizeof(candidate), "%.*s/%s",
							   (int)seg_len, dir, file);
		if (written > 0 && (size_t)written < sizeof(candidate)) {
			execve(candidate, argv, environ);
			if (errno != ENOENT && errno != ENOTDIR) {
				last_errno = errno;
				break;
			}
			last_errno = errno;
		} else if (last_errno == 0) {
			last_errno = ENAMETOOLONG;
		}

		segment = next ? next + 1 : NULL;
	}

	if (last_errno == 0) {
		last_errno = ENOENT;
	}
	errno = last_errno;
	return -1;
}

static void print_prompt(void)
{
	char cwd[PATH_MAX];

	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("%s$ ", cwd);
	} else {
		printf("shell$ ");
	}
	fflush(stdout);
}

static int parse_line(char *line, char *argv[], int max_args)
{
	int argc = 0;
	char *p = line;

	while (*p != '\0') {
		while (isspace((unsigned char)*p)) {
			p++;
		}

		if (*p == '\0') {
			break;
		}

		if (argc >= max_args - 1) {
			fprintf(stderr, "too many arguments\n");
			break;
		}

		argv[argc++] = p;

		while (*p != '\0' && !isspace((unsigned char)*p)) {
			p++;
		}

		if (*p == '\0') {
			break;
		}

		*p = '\0';
		p++;
	}

	argv[argc] = NULL;
	return argc;
}

static int builtin_cd(int argc, char *argv[])
{
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

static int builtin_pwd(void)
{
	char cwd[PATH_MAX];

	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		fprintf(stderr, "pwd: %s\n", strerror(errno));
		return 1;
	}

	printf("%s\n", cwd);
	return 0;
}

static int builtin_export(int argc, char *argv[])
{
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

static char *expand_env_token(const char *input)
{
	if (!input)
		return NULL;

	size_t len = strlen(input);
	size_t cap = len + 1;
	char *out = malloc(cap);
	if (!out)
		return NULL;

	size_t out_len = 0;
	for (size_t i = 0; i < len; i++) {
		if (input[i] != '$') {
			if (out_len + 1 >= cap) {
				cap = cap * 2 + 16;
				char *grown = realloc(out, cap);
				if (!grown) {
					free(out);
					return NULL;
				}
				out = grown;
			}
			out[out_len++] = input[i];
			continue;
		}

		size_t var_start = i + 1;
		size_t var_len = 0;
		bool braced = false;
		if (var_start < len && input[var_start] == '{') {
			braced = true;
			var_start++;
			while (var_start + var_len < len &&
				   input[var_start + var_len] != '}') {
				var_len++;
			}
			if (var_start + var_len >= len) {
				braced = false;
				var_start = i + 1;
				var_len = 0;
			}
		}

		if (!braced) {
			while (var_start + var_len < len) {
				char c = input[var_start + var_len];
				if (!(isalnum((unsigned char)c) || c == '_'))
					break;
				var_len++;
			}
		}

		if (var_len == 0) {
			if (out_len + 1 >= cap) {
				cap = cap * 2 + 16;
				char *grown = realloc(out, cap);
				if (!grown) {
					free(out);
					return NULL;
				}
				out = grown;
			}
			out[out_len++] = input[i];
			continue;
		}

		char *name = strndup(input + var_start, var_len);
		const char *value = NULL;
		if (name) {
			value = getenv(name);
			free(name);
		}
		if (value) {
			size_t value_len = strlen(value);
			if (out_len + value_len >= cap) {
				cap = out_len + value_len + 16;
				char *grown = realloc(out, cap);
				if (!grown) {
					free(out);
					return NULL;
				}
				out = grown;
			}
			memcpy(out + out_len, value, value_len);
			out_len += value_len;
		}

		i = var_start + var_len - 1;
		if (braced)
			i++;
	}

	out[out_len] = '\0';
	return out;
}

static int builtin_echo(int argc, char *argv[])
{
	for (int i = 1; i < argc; i++) {
		char *expanded = expand_env_token(argv[i]);
		const char *out = expanded ? expanded : argv[i];
		if (i > 1)
			putchar(' ');
		fputs(out, stdout);
		free(expanded);
	}
	putchar('\n');
	return 0;
}

static int builtin_cat(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "cat: usage: cat FILE...\n");
		return 1;
	}

	int status = 0;
	char buf[4096];
	for (int i = 1; i < argc; i++) {
		int fd = open(argv[i], O_RDONLY, 0);
		if (fd < 0) {
			fprintf(stderr, "cat: %s: %s\n", argv[i], strerror(errno));
			status = 1;
			continue;
		}

		for (;;) {
			ssize_t n = read(fd, buf, sizeof(buf));
			if (n == 0)
				break;
			if (n < 0) {
				fprintf(stderr, "cat: %s: %s\n", argv[i], strerror(errno));
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
				off += wrote;
			}
			if (off < n)
				break;
		}

		close(fd);
	}

	return status;
}

static int builtin_help(void)
{
	puts("builtins: cd pwd export unset ls cat clear echo env exit help");
	return 0;
}

static int builtin_env(void)
{
	if (!environ)
		return 0;
	for (char **entry = environ; *entry; entry++) {
		puts(*entry);
	}
	return 0;
}

static int builtin_clear(void)
{
	fputs("\033[2J\033[H", stdout);
	fflush(stdout);
	return 0;
}

static void ls_print_entry(const char *name)
{
	if (!name || name[0] == '\0')
		return;
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return;
	printf("%s  ", name);
}

static int builtin_ls(int argc, char *argv[])
{
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

static int builtin_unset(int argc, char *argv[])
{
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

static bool is_builtin(const char *cmd)
{
	return strcmp(cmd, "cd") == 0 || strcmp(cmd, "pwd") == 0 ||
		   strcmp(cmd, "exit") == 0 || strcmp(cmd, "export") == 0 ||
		   strcmp(cmd, "unset") == 0 || strcmp(cmd, "ls") == 0 ||
		   strcmp(cmd, "cat") == 0 || strcmp(cmd, "echo") == 0 ||
		   strcmp(cmd, "help") == 0 || strcmp(cmd, "clear") == 0 ||
		   strcmp(cmd, "env") == 0;
}

static int run_builtin(int argc, char *argv[], bool *should_exit)
{
	if (strcmp(argv[0], "cd") == 0) {
		return builtin_cd(argc, argv);
	}

	if (strcmp(argv[0], "pwd") == 0) {
		return builtin_pwd();
	}

	if (strcmp(argv[0], "export") == 0) {
		return builtin_export(argc, argv);
	}

	if (strcmp(argv[0], "unset") == 0) {
		return builtin_unset(argc, argv);
	}

	if (strcmp(argv[0], "ls") == 0) {
		return builtin_ls(argc, argv);
	}

	if (strcmp(argv[0], "cat") == 0) {
		return builtin_cat(argc, argv);
	}

	if (strcmp(argv[0], "echo") == 0) {
		return builtin_echo(argc, argv);
	}

	if (strcmp(argv[0], "help") == 0) {
		return builtin_help();
	}

	if (strcmp(argv[0], "clear") == 0) {
		return builtin_clear();
	}

	if (strcmp(argv[0], "env") == 0) {
		return builtin_env();
	}

	if (strcmp(argv[0], "exit") == 0) {
		*should_exit = true;

		if (argc >= 2) {
			return atoi(argv[1]);
		}
		return 0;
	}

	return 1;
}

static int run_external(char *argv[])
{
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "fork: %s\n", strerror(errno));
		return 1;
	}

	if (pid == 0) {
		exec_with_path(argv[0], argv);
		fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
		_exit(errno == ENOENT ? 127 : 126);
	}

	int status;
	while (waitpid(pid, &status, 0) < 0) {
		if (errno == EINTR) {
			continue;
		}
		fprintf(stderr, "waitpid: %s\n", strerror(errno));
		return 1;
	}

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}

	if (WIFSIGNALED(status)) {
		fprintf(stderr, "terminated by signal %d\n", WTERMSIG(status));
		return 128 + WTERMSIG(status);
	}

	return 1;
}

int main(void)
{
	char *line = NULL;
	size_t cap = 0;
	int last_status = 0;

	for (;;) {
		print_prompt();

		ssize_t nread = getline(&line, &cap, stdin);
		if (nread < 0) {
			if (feof(stdin)) {
				putchar('\n');
				break;
			}
			fprintf(stderr, "getline: %s\n", strerror(errno));
			continue;
		}

		if (nread > 0 && line[nread - 1] == '\n') {
			line[nread - 1] = '\0';
		}

		char *argv[MAX_ARGS];
		int argc = parse_line(line, argv, MAX_ARGS);

		if (argc == 0) {
			continue;
		}

		bool should_exit = false;

		if (is_builtin(argv[0])) {
			last_status = run_builtin(argc, argv, &should_exit);
			if (should_exit) {
				break;
			}
		} else {
			last_status = run_external(argv);
		}

		(void)last_status;
	}

	free(line);
	return 0;
}
