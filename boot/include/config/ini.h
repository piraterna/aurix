/*********************************************************************************/
/* Module Name:  ini.h                                                           */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#ifndef _CONFIG_INI_H
#define _CONFIG_INI_H

enum token_type {
	SECTION,
	KEY,
	VALUE,

	EOF,
	ILLEGAL,
};

struct string_view {
	char *data;
	unsigned int len;
};

struct token {
	enum token_type type;
	struct string_view lit;
};

struct token_array {
	struct token *items;
	unsigned int count;
	unsigned int capacity;
};

struct lexer {
	char *input;
	unsigned int pos;
	unsigned int read_pos;
	char ch;
};

struct parser {
	struct token_array *tokens;
	unsigned int pos;
	unsigned int read_pos;
	struct token *cur_token;
};

struct key_value {
	struct string_view key;
	struct string_view value;
};

struct section {
	struct string_view name;
	struct key_value *items;
	unsigned int count;
	unsigned int capacity;
};

struct ini_file {
	struct section root;
	struct section *items;
	unsigned int count;
	unsigned int capacity;
};

void parse_ini(struct ini_file *ini, char *buf);

char *ini_get_value(struct ini_file *ini, char *section, char *key);
int ini_get_value_int(struct ini_file *ini, char *section, char *key);

#endif /* _CONFIG_INI_H */
