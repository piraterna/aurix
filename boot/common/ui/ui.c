/*********************************************************************************/
/* Module Name:  ui.c                                                           */
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
#include <lib/string.h>
#include <ui/framebuffer.h>
#include <ui/mouse.h>
#include <ui/font.h>
#include <ui/ui.h>
#include <axboot.h>

#include <print.h>
#include <stdint.h>

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

	char *top_string = BOOTLOADER_NAME_STR " ver. " BOOTLOADER_VERSION_STR;
	int top_string_w;
	ssfn_bbox(&(ctx->font), top_string, &top_string_w, NULL, NULL, NULL);

	terminal_setcur(ctx, ctx->fb_modes[ctx->current_mode].width / 2 - (top_string_w / 2), ctx->fb_modes[ctx->current_mode].height / 32);
	terminal_print(ctx, top_string);

	return true;
}

void gui_draw(void *mouse_status, void *event)
{
	(void)mouse_status;
	(void)event;
}

void tui_draw(void *mouse_status, void *event)
{
	(void)mouse_status;
	(void)event;
}

void ui_init()
{
	struct ui_context ctx = {0};

	if (!get_framebuffer(&ctx.fb_addr, &ctx.fb_modes, &ctx.total_modes, &ctx.current_mode)) {
		debug("ui_init(): Failed to acquire a framebuffer!\n");
		while (1);
	}

	ctx.ui = config_get_ui_mode();

	debug("Dumping framebuffer information\n");
	debug("--------------------------------\n");
	debug("Address: 0x%llx\n", ctx.fb_addr);

	for (int i = 0; i < ctx.total_modes; i++) {
		debug("Mode %u:%s | ", i, (i == ctx.current_mode) ? " (current)" : "");
		debug("Resolution: %ux%u | ", ctx.fb_modes[i].width, ctx.fb_modes[i].height);
		debug("Bits Per Pixel: %u | ", ctx.fb_modes[i].bpp);
		debug("Pitch: %u | ", ctx.fb_modes[i].pitch);
		debug("Format: %s\n", ctx.fb_modes[i].format == FB_RGBA ? "RGBA" : "BGRA");
	}

	ctx.font_buf.ptr = (uint8_t *)ctx.fb_addr;
	ctx.font_buf.w = ctx.fb_modes[ctx.current_mode].width;
	ctx.font_buf.h = ctx.fb_modes[ctx.current_mode].height;
	ctx.font_buf.p = ctx.fb_modes[ctx.current_mode].pitch;
	ctx.font_buf.x = 0;
	ctx.font_buf.y = 0;
	ctx.font_buf.fg = 0xFFFFFFFF;

	void (*ui_callback)(void*,void*) = NULL;

	switch (ctx.ui) {
		case UI_MODERN: {
			if (!gui_init(&ctx)) {
				debug("ui_init(): Failed to initialize modern UI, booting default selection...\n");
				break;
			}
			ui_callback = gui_draw;
			break;
		}
		default:
		case UI_TEXT: {
			if (!tui_init(&ctx)) {
				debug("ui_init(): Failed to initialize text UI, booting default selection...\n");
				break;
			}
			ui_callback = tui_draw;
			break;
		}
	}

	while (1) {
		ui_callback(NULL, NULL);
		//get_mouse(&m_x, &m_y, &m_but);
		//debug("Mouse X = %u | Mouse Y = %u\n", m_x, m_y);
	}
}
