#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"

char *expand_env_token(const char *input)
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

int parse_line(char *line, char *argv[], int max_args)
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

int split_pipeline(char *line, char *segments[], int max_segments)
{
	if (!line || !segments || max_segments <= 0)
		return 0;

	int count = 0;
	char *p = line;
	while (isspace((unsigned char)*p))
		p++;
	if (*p == '\0')
		return 0;

	segments[count++] = p;

	bool in_quote = false;
	char quote = '\0';
	while (*p != '\0') {
		char c = *p;
		if (in_quote) {
			if (c == quote) {
				in_quote = false;
				p++;
				continue;
			}
			if (quote == '"' && c == '\\' && p[1] != '\0') {
				char next = p[1];
				if (next == '"' || next == '\\') {
					p += 2;
					continue;
				}
			}
			p++;
			continue;
		}

		if (c == '"' || c == '\'') {
			in_quote = true;
			quote = c;
			p++;
			continue;
		}

		if (c == '\\' && p[1] != '\0') {
			p += 2;
			continue;
		}

		if (c == '|') {
			*p = '\0';
			p++;
			while (isspace((unsigned char)*p))
				p++;
			if (*p == '\0')
				return -1;
			if (count >= max_segments)
				return -1;
			segments[count++] = p;
			continue;
		}

		p++;
	}

	return count;
}

int split_and_list(char *line, char *segments[], int max_segments)
{
	if (!line || !segments || max_segments <= 0)
		return 0;

	int count = 0;
	char *p = line;
	while (isspace((unsigned char)*p))
		p++;
	if (*p == '\0')
		return 0;

	segments[count++] = p;

	bool in_quote = false;
	char quote = '\0';
	while (*p != '\0') {
		char c = *p;
		if (in_quote) {
			if (c == quote) {
				in_quote = false;
				p++;
				continue;
			}
			if (quote == '"' && c == '\\' && p[1] != '\0') {
				char next = p[1];
				if (next == '"' || next == '\\') {
					p += 2;
					continue;
				}
			}
			p++;
			continue;
		}

		if (c == '"' || c == '\'') {
			in_quote = true;
			quote = c;
			p++;
			continue;
		}

		if (c == '\\' && p[1] != '\0') {
			p += 2;
			continue;
		}

		if (c == '&' && p[1] == '&') {
			*p = '\0';
			p[1] = '\0';
			p += 2;
			while (isspace((unsigned char)*p))
				p++;
			if (*p == '\0')
				return -1;
			if (count >= max_segments)
				return -1;
			segments[count++] = p;
			continue;
		}

		p++;
	}

	return count;
}
