#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"
#include "line.h"
#include "prompt.h"

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

ssize_t sh_readline(char **line, size_t *cap)
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
				if (cursor == len) {
					fputs("\b \b", stdout);
					fflush(stdout);
				} else {
					sh_line_redraw(*line, len, cursor);
				}
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
