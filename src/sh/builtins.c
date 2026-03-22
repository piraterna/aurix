#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
	X(exit, builtin_exit)

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
		char *expanded = expand_env_token(argv[i]);
		const char *raw = expanded ? expanded : argv[i];
		char *escaped = NULL;
		const char *out = raw;
		if (enable_escapes) {
			escaped = expand_echo_escapes(raw);
			if (escaped)
				out = escaped;
		}
		if (i > start)
			putchar(' ');
		fputs(out, stdout);
		free(escaped);
		free(expanded);
	}
	putchar('\n');
	return 0;
}

static int builtin_cat(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 2) {
		fprintf(stderr, "cat: usage: cat FILE...\n");
		return 1;
	}

	int status = 0;
	char buf[4096];
	bool wrote_any = false;
	char last_char = '\n';
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
				wrote_any = true;
				last_char = buf[off + wrote - 1];
				off += wrote;
			}
			if (off < n)
				break;
		}

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
