/*********************************************************************************/
/* Module Name:  ui.c */
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
#include <i18n.h>
#include <lib/string.h>
#include <loader/loader.h>
#include <time/dt.h>
#include <ui/font.h>
#include <ui/framebuffer.h>
#include <ui/keyboard.h>
#include <ui/mouse.h>
#include <ui/ui.h>

#include <print.h>
#include <stdint.h>

enum {
	EVENT_TIME = (1 << 0),
	EVENT_TIMEOUT = (1 << 1),
	EVENT_MOUSE = (1 << 2),

	EVENT_NEXT_ENTRY = (1 << 3),
	EVENT_PREV_ENTRY = (1 << 4),
	EVENT_NEXT_TAB = (1 << 5),

	EVENT_LANG_CHANGE = (1 << 6),
};

struct datetime last_dt;

bool gui_init(struct ui_context *ctx)
{
	if (!font_init(ctx, "\\AxBoot\\fonts\\vera\\Vera.sfn", 16)) {
		return false;
	}

	return false;
}

bool tui_init(struct ui_context *ctx)
{
	if (!font_init(ctx, "\\AxBoot\\fonts\\u_vga16\\u_vga16.sfn", 16)) {
		return false;
	}

	int width = ctx->fb_modes[ctx->current_mode].width;
	int height = ctx->fb_modes[ctx->current_mode].height;
	int width_center = width / 2;
	int height_center = height / 2;

	int w, h;

	// display string at the top center of the screen
	char *top_string = BOOTLOADER_NAME_STR " v" BOOTLOADER_VERSION_STR;
	ssfn_bbox(&(ctx->font), top_string, &w, NULL, NULL, NULL);

	terminal_setcur(ctx, width_center - (w / 2), height / 32);
	terminal_print(ctx, top_string);

	// display current language
	char lang_str[64];
	snprintf((char *)&lang_str, 64, "%s: %s", i18n_curlang->lang->language,
			 i18n_curlang->name);
	ssfn_bbox(&(ctx->font), (char *)lang_str, &w, &h, NULL, NULL);
	terminal_setcur(ctx, width - w - ctx->font.size, height - h);
	terminal_print(ctx, (char *)lang_str);

	// display all entries
	int entry_count = config_get_entry_count();
	int entry_height_offset = height_center - ((h * entry_count) / 2);
	struct axboot_entry *entries = config_get_entries();

	for (int i = 0; i < entry_count; i++) {
		debug("tui_init(): Drawing entries... (%u/%u)\n", i, entry_count);
		ssfn_bbox(&(ctx->font), entries[i].name, &w, &h, NULL, NULL);
		terminal_setcur(ctx, width_center - (w / 2),
						entry_height_offset + (i * h));
		terminal_print(ctx, entries[i].name);
	}

	// highlight default entry
	int default_entry = ctx->current_selection;
	char final_selent[128];
	int f_w, f_h;
	ssfn_bbox(&(ctx->font), " ", &f_w, &f_h, NULL, NULL);
	snprintf((char *)&final_selent, 128, "> %s <", entries[default_entry].name);
	ssfn_bbox(&(ctx->font), entries[default_entry].name, &w, &h, NULL, NULL);
	terminal_setcur(ctx, width_center - (w / 2) - (f_w * 2),
					entry_height_offset + (default_entry * h));
	terminal_print(ctx, "> ");
	terminal_setcur(ctx, width_center + (w / 2),
					entry_height_offset + (default_entry * h));
	terminal_print(ctx, " <");

	return true;
}

void gui_draw(struct ui_context *ctx, struct datetime *dt,
			  struct language_selection *newlang,
			  struct mouse_event *mouse_status, uint8_t event)
{
	(void)ctx;
	(void)dt;
	(void)newlang;
	(void)mouse_status;
	(void)event;
}

void tui_draw(struct ui_context *ctx, struct datetime *dt,
			  struct language_selection *newlang,
			  struct mouse_event *mouse_status, uint8_t event)
{
	(void)mouse_status;

	// display the current date and time at the bottom left corner of the screen
	if (event & EVENT_TIME) {
		int w, h;
		char dt_str[20] = { 0 }; // YYYY/mm/dd HH:MM:SS
		snprintf((char *)&dt_str, 20, "%.4u/%.2u/%.2u %.2u:%.2u:%.2u", dt->year,
				 dt->month, dt->day, dt->h, dt->m, dt->s);

		ssfn_bbox(&(ctx->font), (char *)dt_str, &w, &h, NULL, NULL);

		for (uint32_t y = ctx->fb_modes[ctx->current_mode].height - (2 * h);
			 y < ctx->fb_modes[ctx->current_mode].height - h; y++) {
			for (int x = 0; x < w; x++) {
				*((uint32_t *)ctx->fb_addr +
				  (ctx->fb_modes[ctx->current_mode].pitch /
				   ctx->fb_modes[ctx->current_mode].bpp) *
					  y +
				  x) = 0xFF000000;
			}
		}
		terminal_setcur(ctx, 0, ctx->fb_modes[ctx->current_mode].height - h);
		terminal_print(ctx, (char *)dt_str);

		// update last datetime
		last_dt = *dt;
	}

	// timeout update
	if (event & EVENT_TIMEOUT) {
		// TODO
	}

	// select next entry
	if (event & EVENT_NEXT_ENTRY) {
		// TODO
	}

	// select previous entry
	if (event & EVENT_PREV_ENTRY) {
		// TODO
	}

	// update language
	if (event & EVENT_LANG_CHANGE) {
		int w, h;
		char curlang_str[64];
		snprintf((char *)&curlang_str, 64, "%s: %s",
				 i18n_curlang->lang->language);
		ssfn_bbox(&(ctx->font), (char *)curlang_str, &w, &h, NULL, NULL);
		for (uint32_t y = ctx->fb_modes[ctx->current_mode].height - (2 * h);
			 y < ctx->fb_modes[ctx->current_mode].height - h; y++) {
			for (int x = 0; x < w; x++) {
				*((uint32_t *)ctx->fb_addr +
				  (ctx->fb_modes[ctx->current_mode].pitch /
				   ctx->fb_modes[ctx->current_mode].bpp) *
					  y +
				  x) = 0xFF000000;
			}
		}

		i18n_curlang = newlang;

		char newlang_str[64];
		snprintf((char *)&curlang_str, 64, "%s: %s",
				 i18n_curlang->lang->language);
		ssfn_bbox(&(ctx->font), (char *)curlang_str, &w, &h, NULL, NULL);
		terminal_setcur(ctx, ctx->fb_modes[ctx->current_mode].width - w,
						ctx->fb_modes[ctx->current_mode].height - h);
		terminal_print(ctx, (char *)newlang_str);
	}
}

void ui_init()
{
	struct ui_context ctx = { 0 };

	if (!get_framebuffer(&ctx.fb_addr, &ctx.fb_modes, &ctx.total_modes,
						 &ctx.current_mode)) {
		debug("ui_init(): Failed to acquire a framebuffer!\n");
		while (1)
			;
	}

	ctx.ui = config_get_ui_mode();

	debug("Dumping framebuffer information\n");
	debug("--------------------------------\n");
	debug("Address: 0x%llx\n", ctx.fb_addr);

	for (int i = 0; i < ctx.total_modes; i++) {
		debug("Mode %u:%s | ", i, (i == ctx.current_mode) ? " (current)" : "");
		debug("Resolution: %ux%u | ", ctx.fb_modes[i].width,
			  ctx.fb_modes[i].height);
		debug("Bytes Per Pixel: %u | ", ctx.fb_modes[i].bpp);
		debug("Pitch: %u | ", ctx.fb_modes[i].pitch);
		debug("Format: %s\n",
			  ctx.fb_modes[i].format == FB_RGBA ? "RGBA" : "BGRA");
	}

	ctx.font_buf.ptr = (uint8_t *)ctx.fb_addr;
	ctx.font_buf.w = ctx.fb_modes[ctx.current_mode].width;
	ctx.font_buf.h = ctx.fb_modes[ctx.current_mode].height;
	ctx.font_buf.p = ctx.fb_modes[ctx.current_mode].pitch;
	ctx.font_buf.x = 0;
	ctx.font_buf.y = 0;
	ctx.font_buf.fg = 0xFFFFFFFF;

	ctx.current_selection = config_get_default();

	void (*ui_callback)(struct ui_context *, struct datetime *,
						struct language_selection *, struct mouse_event *,
						uint8_t) = NULL;

	switch (ctx.ui) {
	case UI_MODERN: {
		if (!gui_init(&ctx)) {
			debug(
				"ui_init(): Failed to initialize modern UI, booting default selection...\n");
			break;
		}
		ui_callback = gui_draw;
		break;
	}
	default:
	case UI_TEXT: {
		if (!tui_init(&ctx)) {
			debug(
				"ui_init(): Failed to initialize text UI, booting default selection...\n");
			break;
		}
		ui_callback = tui_draw;
		break;
	}
	}

	struct datetime dt;
	struct mouse_event me;

	while (1) {
		uint8_t event = 0;

		// datetime?
		get_datetime(&dt);
		if (memcmp(&dt, &last_dt, sizeof(struct datetime)) != 0) {
			event |= EVENT_TIME;
		}

		// mouse movement?
		get_mouse(&(me.x), &(me.y), &(me.but));
		if (memcmp(&me, &(ctx.last_mouse), sizeof(struct mouse_event)) != 0) {
			event |= EVENT_MOUSE;
		}

		// keypress?
		uint16_t scancode;
		get_keypress(&scancode);
		if (scancode != 0) {
			switch (scancode) {
			case SCANCODE_ARROW_DOWN:
				event |= EVENT_NEXT_ENTRY;
				break;
			case SCANCODE_ARROW_UP:
				event |= EVENT_PREV_ENTRY;
				break;
			case SCANCODE_ENTER:
				// clear the screen
				for (uint32_t y = 0; y < ctx.fb_modes[ctx.current_mode].height;
					 y++) {
					for (uint32_t x = 0;
						 x < ctx.fb_modes[ctx.current_mode].width; x++) {
						*((uint32_t *)ctx.fb_addr +
						  (ctx.fb_modes[ctx.current_mode].pitch /
						   ctx.fb_modes[ctx.current_mode].bpp) *
							  y +
						  x) = 0xFF000000;
					}
				}
				struct axboot_entry *entries = config_get_entries();
				loader_load(&entries[ctx.current_selection]);
				break;
			default:
				break;
			}
		}

		if (event != 0) {
			ui_callback(&ctx, &dt, NULL, &me, event);
		} else {
#ifdef __x86_64
			__asm__ volatile("hlt");
#endif
			// arch_wait();
		}
	}
}
