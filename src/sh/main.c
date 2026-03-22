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
#define SH_HISTORY_MAX 64

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
		if (getcwd(cwd, sizeof(cwd)) != NULL) {
			const char *home = getenv("HOME");
			if (home && strcmp(cwd, home) == 0)
				cwd_value = "~";
			else
				cwd_value = cwd;
		} else {
			cwd_value = "?";
		}
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

static char *sh_history[SH_HISTORY_MAX];
static size_t sh_history_count;
static size_t sh_history_index;
static bool sh_history_active;
static char *sh_history_saved;

static void sh_history_clear_saved(void)
{
	free(sh_history_saved);
	sh_history_saved = NULL;
}

static void sh_history_add(const char *line)
{
	if (!line || line[0] == '\0')
		return;
	if (sh_history_count > 0 &&
		strcmp(sh_history[sh_history_count - 1], line) == 0)
		return;
	if (sh_history_count == SH_HISTORY_MAX) {
		free(sh_history[0]);
		memmove(sh_history, sh_history + 1,
				(SH_HISTORY_MAX - 1) * sizeof(*sh_history));
		sh_history_count--;
	}
	sh_history[sh_history_count] = strdup(line);
	if (sh_history[sh_history_count])
		sh_history_count++;
	sh_history_active = false;
}

static const char *sh_history_prev(const char *current)
{
	if (sh_history_count == 0)
		return NULL;
	if (!sh_history_active) {
		sh_history_clear_saved();
		sh_history_saved = strdup(current ? current : "");
		sh_history_active = true;
		sh_history_index = sh_history_count - 1;
	} else if (sh_history_index > 0) {
		sh_history_index--;
	}
	return sh_history[sh_history_index];
}

static const char *sh_history_next(void)
{
	if (!sh_history_active)
		return NULL;
	if (sh_history_index + 1 < sh_history_count) {
		sh_history_index++;
		return sh_history[sh_history_index];
	}
	sh_history_active = false;
	return sh_history_saved ? sh_history_saved : "";
}

static void sh_line_redraw(const char *line, size_t len, size_t cursor)
{
	fputs("\r\033[K", stdout);
	print_prompt();
	if (len > 0)
		fwrite(line, 1, len, stdout);
	if (cursor < len) {
		size_t back = len - cursor;
		if (back > 0)
			fprintf(stdout, "\033[%zuD", back);
	}
	fflush(stdout);
}

static ssize_t sh_readline(char **line, size_t *cap)
{
	if (!line || !cap)
		return -1;
	if (!*line || *cap == 0) {
		*cap = 128;
		*line = malloc(*cap);
		if (!*line)
			return -1;
	}
	(*line)[0] = '\0';

	size_t len = 0;
	size_t cursor = 0;
	bool in_escape = false;
	char esc_buf[2];
	int esc_pos = 0;

	for (;;) {
		int c = getchar();
		if (c == EOF) {
			if (len == 0)
				return -1;
			break;
		}

		if (in_escape) {
			esc_buf[esc_pos++] = (char)c;
			if (esc_pos == 1) {
				if (esc_buf[0] != '[') {
					in_escape = false;
					esc_pos = 0;
				}
				continue;
			}
			if (esc_pos == 2) {
				const char *hist = NULL;
				if (esc_buf[1] == 'A')
					hist = sh_history_prev(*line);
				else if (esc_buf[1] == 'B')
					hist = sh_history_next();
				else if (esc_buf[1] == 'C') {
					if (cursor < len) {
						cursor++;
						fputs("\033[C", stdout);
						fflush(stdout);
					}
				} else if (esc_buf[1] == 'D') {
					if (cursor > 0) {
						cursor--;
						fputs("\033[D", stdout);
						fflush(stdout);
					}
				}
				if (hist) {
					size_t hlen = strlen(hist);
					if (hlen + 1 > *cap) {
						char *tmp = realloc(*line, hlen + 1);
						if (tmp) {
							*line = tmp;
							*cap = hlen + 1;
						}
					}
					memcpy(*line, hist, hlen + 1);
					len = hlen;
					cursor = len;
					sh_line_redraw(*line, len, cursor);
				}
				in_escape = false;
				esc_pos = 0;
			}
			continue;
		}

		if (c == '\033') {
			in_escape = true;
			esc_pos = 0;
			continue;
		}

		if (c == '\n') {
			fputc('\n', stdout);
			break;
		}

		if (c == '\b' || c == 0x7F) {
			if (cursor > 0) {
				memmove(*line + cursor - 1, *line + cursor, len - cursor + 1);
				cursor--;
				len--;
				sh_line_redraw(*line, len, cursor);
			}
			continue;
		}

		if (len + 1 >= *cap) {
			size_t next = *cap * 2;
			char *tmp = realloc(*line, next);
			if (!tmp)
				return -1;
			*line = tmp;
			*cap = next;
		}
		if (cursor == len) {
			(*line)[len++] = (char)c;
			(*line)[len] = '\0';
			cursor = len;
			fputc(c, stdout);
			fflush(stdout);
		} else {
			memmove(*line + cursor + 1, *line + cursor, len - cursor + 1);
			(*line)[cursor] = (char)c;
			len++;
			cursor++;
			sh_line_redraw(*line, len, cursor);
		}
	}

	if (len > 0 && (*line)[len - 1] == '\n')
		(*line)[len - 1] = '\0';
	return (ssize_t)len;
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

	char *expanded[MAX_ARGS];
	bool expanded_alloc[MAX_ARGS];
	for (int i = 0; i < argc; i++) {
		expanded[i] = expand_env_token(argv[i]);
		if (expanded[i]) {
			expanded_alloc[i] = true;
		} else {
			expanded[i] = argv[i];
			expanded_alloc[i] = false;
		}
	}
	expanded[argc] = NULL;

	const struct builtin *builtin = builtin_lookup(expanded[0]);
	if (builtin) {
		*last_status = builtin->handler(argc, expanded, should_exit);
		for (int i = 0; i < argc; i++) {
			if (expanded_alloc[i])
				free(expanded[i]);
		}
		return 0;
	}

	*last_status = run_external(expanded);
	for (int i = 0; i < argc; i++) {
		if (expanded_alloc[i])
			free(expanded[i]);
	}
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

		ssize_t nread = sh_readline(&line, &cap);
		if (nread < 0) {
			if (feof(stdin)) {
				putchar('\n');
				break;
			}
			fprintf(stderr, "readline: %s\n", strerror(errno));
			continue;
		}

		if (nread > 0)
			sh_history_add(line);

		should_exit = false;
		(void)run_command_line(line, &last_status, &should_exit);
		if (should_exit)
			break;

		(void)last_status;
	}

	free(line);
	return 0;
}
