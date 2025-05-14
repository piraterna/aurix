/*********************************************************************************/
/* Module Name:  config.h                                                        */
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

#ifndef _CONFIG_CONFIG_H
#define _CONFIG_CONFIG_H

enum {
	UI_TEXT = 0,
	UI_MODERN = 1
};

struct axboot_cfg {
	// overridable stuff
	int default_entry;
	int timeout;
	int ui_mode;

	int entry_count;
};

struct axboot_entry {
	char *name;
	char *description;
	char *image_path;
	int protocol;
};

void config_init(void);

int config_get_timeout();
int config_get_default();
int config_get_entry_count();
struct axboot_entry *config_get_entries();
int config_get_ui_mode();

#endif /* _CONFIG_CONFIG_H */
