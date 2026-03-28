#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "exec.h"
#include "history.h"
#include "line.h"
#include "prompt.h"

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
