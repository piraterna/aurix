#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "prompt.h"

void print_prompt(void)
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
