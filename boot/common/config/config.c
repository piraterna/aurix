/*********************************************************************************/
/* Module Name:  config.c */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
/*********************************************************************************/

#include <axboot.h>
#include <config/config.h>
#include <lib/string.h>
#include <loader/loader.h>
#include <mm/mman.h>
#include <print.h>
#include <time/dt.h>
#include <vfs/vfs.h>

#include <stddef.h>
#include <stdint.h>

#define DEFAULT_ENTRY 0
// default timeout of 0 disables the UI entirely, which is essentialy what
// *should* happen now since the UI is... in a catastrophic state. Just remember
// to set this back to 30 once the UI is ready
#define DEFAULT_TIMEOUT 0
// #define DEFAULT_TIMEOUT 30

char *config_paths[] = {
	"\\AxBoot\\axboot.cfg",
	"\\axboot.cfg",
	"\\EFI\\axboot.cfg",
	"\\EFI\\BOOT\\axboot.cfg",
};

struct axboot_cfg cfg = { .default_entry = DEFAULT_ENTRY,
						  .timeout = DEFAULT_TIMEOUT,
						  .ui_mode = UI_TEXT,

						  //.entry_count = 0,
						  .entry_count = 2,
						  .bootlog_filename = NULL,
						  .modules = { "\\System\\support\\dummy.sys", NULL } };

struct axboot_entry entries[2] = {
	{ .name = "AurixOS",
	  .description = "Boot the Aurix Operating System",
	  .image_path = "\\System\\axkrnl",
	  .protocol = PROTO_AURIX },
	{ .name = "Windows 10",
	  .description = "",
	  .image_path = "\\EFI\\Microsoft\\bootmgfw.efi",
	  .protocol = PROTO_CHAINLOAD }
};

void config_init(void)
{
	// create a filename for boot log
	// format: \AXBOOT_LOG-YY-MM-DD_HHMMSS.txt
	char bootlog_fn[33];
	struct datetime dt;
	get_datetime(&dt);

	snprintf(bootlog_fn, 33, "\\AXBOOT_LOG-%u-%u-%u_%u%u%u.txt", dt.year,
			 dt.month, dt.day, dt.h, dt.m, dt.s);
	cfg.bootlog_filename = (char *)mem_alloc(ARRAY_LENGTH(bootlog_fn));
	if (!cfg.bootlog_filename) {
		debug("Error!\n");
	} else {
		strncpy(cfg.bootlog_filename, (char *)&bootlog_fn, 33);
	}

	char *config_buf = NULL;
	size_t config_len = 0;
	uint8_t open = 0;

	for (size_t i = 0; i < ARRAY_LENGTH(config_paths); i++) {
		vfs_read(config_paths[i], &config_buf, &config_len);
		if (config_buf != NULL) {
			open = 1;
			break;
		}
	}

	if (open == 0) {
		debug(
			"config_init(): Couldn't open a configuration file! Entering console...\n");
		// console();
		while (1)
			;
	}

	mem_free(config_buf);
}

int config_get_timeout()
{
	return cfg.timeout;
}

int config_get_default()
{
	return cfg.default_entry;
}

int config_get_entry_count()
{
	return cfg.entry_count;
}

struct axboot_entry *config_get_entries()
{
	return entries;
}

int config_get_ui_mode()
{
	return cfg.ui_mode;
}

char **config_get_modules(uint32_t *count)
{
	if (count) {
		*count = 0;
		while (cfg.modules[*count]) {
			*count += 1;
		}
	}

	return cfg.modules;
}