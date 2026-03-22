#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 128

extern char **environ;

#include "builtins.h"

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
	const char *prompt = getenv("PS1");
	if (!prompt || prompt[0] == '\0')
		prompt = "$ ";

	const char *expanded_prompt = prompt;
	char *escaped = NULL;
	if (strchr(prompt, '\\') != NULL) {
		escaped = malloc(strlen(prompt) + 1);
		if (escaped) {
			size_t out_len = 0;
			for (size_t i = 0; prompt[i] != '\0'; i++) {
				if (prompt[i] != '\\') {
					escaped[out_len++] = prompt[i];
					continue;
				}

				if (prompt[i + 1] == '\0') {
					escaped[out_len++] = '\\';
					break;
				}

				char c = prompt[++i];
				switch (c) {
				case 'e':
					escaped[out_len++] = '\033';
					break;
				case 'n':
					escaped[out_len++] = '\n';
					break;
				case 't':
					escaped[out_len++] = '\t';
					break;
				case 'r':
					escaped[out_len++] = '\r';
					break;
				case 'a':
					escaped[out_len++] = '\a';
					break;
				case 'b':
					escaped[out_len++] = '\b';
					break;
				case 'f':
					escaped[out_len++] = '\f';
					break;
				case 'v':
					escaped[out_len++] = '\v';
					break;
				case '\\':
					escaped[out_len++] = '\\';
					break;
				case 'x': {
					int value = 0;
					int digits = 0;
					while (prompt[i + 1] != '\0' && digits < 2 &&
						   isxdigit((unsigned char)prompt[i + 1])) {
						char h = prompt[i + 1];
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
						escaped[out_len++] = (char)value;
					else
						escaped[out_len++] = 'x';
					break;
				}
				case '0': {
					int value = 0;
					int digits = 0;
					while (prompt[i + 1] != '\0' && digits < 3) {
						char o = prompt[i + 1];
						if (o < '0' || o > '7')
							break;
						value = value * 8 + (o - '0');
						i++;
						digits++;
					}
					escaped[out_len++] = (char)value;
					break;
				}
				default:
					escaped[out_len++] = c;
					break;
				}
			}
			escaped[out_len] = '\0';
			expanded_prompt = escaped;
		}
	}

	char cwd[PATH_MAX];
	const char *cwd_value = NULL;
	if (strstr(expanded_prompt, "%p") != NULL) {
		if (getcwd(cwd, sizeof(cwd)) != NULL)
			cwd_value = cwd;
		else
			cwd_value = "?";
	}

	for (const char *p = expanded_prompt; *p != '\0'; p++) {
		if (p[0] == '%' && p[1] != '\0') {
			if (p[1] == 'p') {
				if (cwd_value)
					fputs(cwd_value, stdout);
				p++;
				continue;
			}
			if (p[1] == '%') {
				fputc('%', stdout);
				p++;
				continue;
			}
		}
		fputc(*p, stdout);
	}
	free(escaped);
	fflush(stdout);
}

static int parse_line(char *line, char *argv[], int max_args)
{
	int argc = 0;
	char *p = line;
	char *out = line;
	bool in_token = false;
	bool in_quote = false;
	char quote = '\0';

	while (*p != '\0') {
		char c = *p;

		if (!in_token) {
			if (isspace((unsigned char)c)) {
				p++;
				continue;
			}
			if (argc >= max_args - 1) {
				fprintf(stderr, "too many arguments\n");
				break;
			}
			argv[argc++] = out;
			in_token = true;
		}

		if (in_quote) {
			if (c == quote) {
				in_quote = false;
				p++;
				continue;
			}
			if (quote == '"' && c == '\\' && p[1] != '\0') {
				char next = p[1];
				if (next == '"' || next == '\\') {
					p++;
					c = *p;
				}
			}
			*out++ = c;
			p++;
			continue;
		}

		if (c == '"' || c == '\'') {
			in_quote = true;
			quote = c;
			p++;
			continue;
		}

		if (isspace((unsigned char)c)) {
			*out++ = '\0';
			in_token = false;
			p++;
			continue;
		}

		if (c == '\\' && p[1] != '\0') {
			char next = p[1];
			if (isspace((unsigned char)next) || next == '\\' || next == '"' ||
				next == '\'') {
				p++;
				c = *p;
			} else {
				*out++ = '\\';
				p++;
				c = *p;
			}
		}

		*out++ = c;
		p++;
	}

	if (in_token) {
		*out++ = '\0';
	}

	argv[argc] = NULL;
	return argc;
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

static int run_command_line(char *line, int *last_status, bool *should_exit)
{
	if (!line || !last_status || !should_exit)
		return 1;

	char *argv[MAX_ARGS];
	int argc = parse_line(line, argv, MAX_ARGS);
	if (argc == 0)
		return 0;

	const struct builtin *builtin = builtin_lookup(argv[0]);
	if (builtin) {
		*last_status = builtin->handler(argc, argv, should_exit);
		return 0;
	}

	*last_status = run_external(argv);
	return 0;
}

static void run_shinit(int *last_status, bool *should_exit)
{
	const char *home = getenv("HOME");
	if (!home || home[0] == '\0')
		return;

	size_t home_len = strlen(home);
	const char *suffix = "/.shinit";
	size_t path_len = home_len + strlen(suffix) + 1;
	char *path = malloc(path_len);
	if (!path)
		return;
	strcpy(path, home);
	strcat(path, suffix);

	FILE *file = fopen(path, "r");
	free(path);
	if (!file) {
		if (errno != ENOENT) {
			fprintf(stderr, "shinit: %s\n", strerror(errno));
		}
		return;
	}

	char *line = NULL;
	size_t cap = 0;
	while (!*should_exit) {
		ssize_t nread = getline(&line, &cap, file);
		if (nread < 0)
			break;

		if (nread > 0 && line[nread - 1] == '\n')
			line[nread - 1] = '\0';

		if (line[0] == '#')
			continue;

		(void)run_command_line(line, last_status, should_exit);
		if (*last_status != 0)
			break;
	}

	free(line);
	fclose(file);
}

int main(void)
{
	char *line = NULL;
	size_t cap = 0;
	int last_status = 0;
	bool should_exit = false;
	const char *home = getenv("HOME");
	if (!home || home[0] == '\0' || chdir(home) != 0)
		chdir("/");

	run_shinit(&last_status, &should_exit);
	if (should_exit) {
		free(line);
		return last_status;
	}

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

		should_exit = false;
		(void)run_command_line(line, &last_status, &should_exit);
		if (should_exit)
			break;

		(void)last_status;
	}

	free(line);
	return 0;
}
