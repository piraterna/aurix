#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(void)
{
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	while (1) {
		printf("> ");
		fflush(stdout);

		read = getline(&line, &len, stdin);
		if (read == -1) {
			if (feof(stdin)) {
				printf("\nEOF received, exiting.\n");
			} else {
				fprintf(stderr, "Error reading input: %s\n", strerror(errno));
			}
			break;
		}

		if (read > 0 && line[read - 1] == '\n') {
			line[read - 1] = '\0';
		}

		if (strcmp(line, "exit") == 0) {
			printf("Exiting REPL.\n");
			break;
		}

		printf("You said: %s\n", line);
	}

	free(line);
	return 0;
}