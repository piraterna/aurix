/*********************************************************************************/
/* Module Name:  config.c                                                        */
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

#include <config/config.h>
#include <config/ini.h>
#include <lib/string.h>
#include <loader/loader.h>
#include <mm/mman.h>
#include <vfs/vfs.h>
#include <print.h>
#include <axboot.h>

#include <stdint.h>
#include <stddef.h>

#define DEFAULT_ENTRY 0
// default timeout of 0 disables the UI entirely, which is essentialy what *should* happen now since the UI
// is... in a catastrophic state. Just remember to set this back to 30 once the UI is ready
#define DEFAULT_TIMEOUT 0
// #define DEFAULT_TIMEOUT 30

char *config_paths[] = {
	"\\AxBoot\\axboot.cfg",
	"\\axboot.cfg",
	"\\EFI\\axboot.cfg",
	"\\EFI\\BOOT\\axboot.cfg",
};

struct axboot_cfg cfg = {
	.default_entry = DEFAULT_ENTRY,
	.timeout = DEFAULT_TIMEOUT,
	.ui_mode = UI_TEXT,

	//.entry_count = 0
	.entry_count = 2
};

struct axboot_entry entries[2] = {
	{
		.name = "AurixOS",
		.description = "Boot the Aurix Operating System",
		.image_path = "\\System\\axkrnl",
		.protocol = PROTO_AURIX
	},
	{
		.name = "Windows 10",
		.description = "",
		.image_path = "\\EFI\\Microsoft\\bootmgfw.efi",
		.protocol = PROTO_CHAINLOAD
	}
};


void config_init(void)
{
	char *config_buf = NULL;
	uint8_t open = 0;
	
	for (size_t i = 0; i < ARRAY_LENGTH(config_paths); i++) {
		vfs_read(config_paths[i], &config_buf);
		if (config_buf != NULL) {
			open = 1;
			break;
		}
	}

	if (open == 0) {
		debug("Couldn't open a configuration file! Entering console...\n");
		//console();
		while (1);
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