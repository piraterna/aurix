#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "exec.h"
#include "parse.h"

#define MAX_ARGS 128
#define MAX_PIPE_CMDS 32

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

static void free_expanded(char *expanded[], bool expanded_alloc[], int argc)
{
	for (int i = 0; i < argc; i++) {
		if (expanded_alloc[i])
			free(expanded[i]);
	}
}

static int build_argv(char *line, char *argv[], char *expanded[],
					  bool expanded_alloc[], int max_args, int *argc_out)
{
	int argc = parse_line(line, argv, max_args);
	if (argc_out)
		*argc_out = argc;
	if (argc == 0)
		return 0;

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
	return 0;
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

static int run_single_command(char *line, int *last_status, bool *should_exit)
{
	char *argv[MAX_ARGS];
	char *expanded[MAX_ARGS];
	bool expanded_alloc[MAX_ARGS];
	int argc = 0;

	(void)build_argv(line, argv, expanded, expanded_alloc, MAX_ARGS, &argc);
	if (argc == 0)
		return 0;

	const struct builtin *builtin = builtin_lookup(expanded[0]);
	if (builtin) {
		*last_status = builtin->handler(argc, expanded, should_exit);
		free_expanded(expanded, expanded_alloc, argc);
		return 0;
	}

	*last_status = run_external(expanded);
	free_expanded(expanded, expanded_alloc, argc);
	return 0;
}

static int run_pipeline(char *segments[], int count, int *last_status)
{
	int pipes[MAX_PIPE_CMDS - 1][2];
	pid_t pids[MAX_PIPE_CMDS];
	pid_t last_pid = -1;
	int startfd[2] = { -1, -1 };

	if (pipe(startfd) != 0) {
		fprintf(stderr, "pipe: %s\n", strerror(errno));
		return 1;
	}

	for (int i = 0; i < count - 1; i++) {
		if (pipe(pipes[i]) != 0) {
			fprintf(stderr, "pipe: %s\n", strerror(errno));
			for (int j = 0; j < i; j++) {
				close(pipes[j][0]);
				close(pipes[j][1]);
			}
			return 1;
		}
	}

	for (int i = count - 1; i >= 0; i--) {
		pid_t pid = fork();
		if (pid < 0) {
			fprintf(stderr, "fork: %s\n", strerror(errno));
			for (int j = 0; j < count - 1; j++) {
				close(pipes[j][0]);
				close(pipes[j][1]);
			}
			return 1;
		}

		if (pid == 0) {
			close(startfd[1]);
			if (i > 0) {
				if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
					fprintf(stderr, "dup2: %s\n", strerror(errno));
					_exit(126);
				}
			}
			if (i < count - 1) {
				if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
					fprintf(stderr, "dup2: %s\n", strerror(errno));
					_exit(126);
				}
			}
			for (int j = 0; j < count - 1; j++) {
				close(pipes[j][0]);
				close(pipes[j][1]);
			}

			char ready = 0;
			(void)read(startfd[0], &ready, 1);
			close(startfd[0]);

			setvbuf(stdout, NULL, _IONBF, 0);
			setvbuf(stderr, NULL, _IONBF, 0);

			char *argv[MAX_ARGS];
			char *expanded[MAX_ARGS];
			bool expanded_alloc[MAX_ARGS];
			int argc = 0;
			(void)build_argv(segments[i], argv, expanded, expanded_alloc,
							 MAX_ARGS, &argc);
			if (argc == 0) {
				fprintf(stderr, "syntax error near unexpected token `|'\n");
				_exit(2);
			}

			const struct builtin *builtin = builtin_lookup(expanded[0]);
			if (builtin) {
				bool child_exit = false;
				int status = builtin->handler(argc, expanded, &child_exit);
				fflush(NULL);
				free_expanded(expanded, expanded_alloc, argc);
				_exit(status);
			}

			exec_with_path(expanded[0], expanded);
			fprintf(stderr, "%s: %s\n", expanded[0], strerror(errno));
			free_expanded(expanded, expanded_alloc, argc);
			_exit(errno == ENOENT ? 127 : 126);
		}

		pids[i] = pid;
		if (i == count - 1)
			last_pid = pid;
	}

	close(startfd[0]);
	for (int i = 0; i < count; i++) {
		char ready = 1;
		if (write(startfd[1], &ready, 1) < 0)
			break;
	}
	close(startfd[1]);

	for (int j = 0; j < count - 1; j++) {
		close(pipes[j][0]);
		close(pipes[j][1]);
	}

	int status = 0;
	for (int i = 0; i < count; i++) {
		int wstatus;
		pid_t pid;
		do {
			pid = waitpid(pids[i], &wstatus, 0);
		} while (pid < 0 && errno == EINTR);
		if (pid < 0) {
			fprintf(stderr, "waitpid: %s\n", strerror(errno));
			status = 1;
			continue;
		}
		if (pid == last_pid) {
			if (WIFEXITED(wstatus))
				status = WEXITSTATUS(wstatus);
			else if (WIFSIGNALED(wstatus))
				status = 128 + WTERMSIG(wstatus);
			else
				status = 1;
		}
	}

	if (last_status)
		*last_status = status;
	return 0;
}

static int run_command_segment(char *line, int *last_status, bool *should_exit)
{
	char *segments[MAX_PIPE_CMDS];
	int seg_count = split_pipeline(line, segments, MAX_PIPE_CMDS);
	if (seg_count < 0) {
		fprintf(stderr, "syntax error near unexpected token `|'\n");
		*last_status = 2;
		return 1;
	}
	if (seg_count == 0)
		return 0;
	if (seg_count == 1)
		return run_single_command(segments[0], last_status, should_exit);

	return run_pipeline(segments, seg_count, last_status);
}

int run_command_line(char *line, int *last_status, bool *should_exit)
{
	if (!line || !last_status || !should_exit)
		return 1;

	char *segments[MAX_PIPE_CMDS];
	int seg_count = split_and_list(line, segments, MAX_PIPE_CMDS);
	if (seg_count < 0) {
		fprintf(stderr, "syntax error near unexpected token `&&'\n");
		*last_status = 2;
		return 1;
	}
	if (seg_count == 0)
		return 0;

	for (int i = 0; i < seg_count; i++) {
		(void)run_command_segment(segments[i], last_status, should_exit);
		if (*last_status != 0)
			break;
		if (*should_exit)
			break;
	}

	return 0;
}

void run_shinit(int *last_status, bool *should_exit)
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
