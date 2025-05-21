/*********************************************************************************/
/* Module Name:  terminal.c                                                      */
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

#include <ui/ui.h>
#include <ui/terminal.h>
#include <axboot.h>
#include <print.h>

#include <stdarg.h>

#define HORIZONTAL_TAB_WIDTH 8

void terminal_print(struct ui_context *ctx, char *fmt, ...)
{
	va_list args;
	char buf[4096] = {0};
	char *s = (char *)&buf;

	va_start(args, fmt);
	vsnprintf((char *)&buf, sizeof(buf), fmt, args);
	va_end(args);

	while (*s) {
		switch (*s) {
			case '\t': {
				// horizontal tab - 4 spaces
				int w, h;
				ssfn_bbox(&(ctx->font), " ", &w, &h, NULL, NULL);
				if (ctx->terminal.cx >= ctx->fb_modes[ctx->current_mode].width - (w * HORIZONTAL_TAB_WIDTH)) {
					for (int i = 1; i <= HORIZONTAL_TAB_WIDTH; i++) {
						if (ctx->terminal.cx >= ctx->fb_modes[ctx->current_mode].width - (w * i)) {
							ctx->terminal.cx = ((HORIZONTAL_TAB_WIDTH * (w + 1)) - (w * i));
							ctx->terminal.cy += h;
							break;
						}
					}
				} else {
					ctx->terminal.cx += w * HORIZONTAL_TAB_WIDTH;
				}
				break;
			}
			case '\n': {
				// newline
				ctx->terminal.cx = 0;
				ctx->terminal.cy += ctx->terminal.font_size;
				break;
			}
			default: {
				// printable character
				const char str[2] = {*s, 0};
				int w, h;
				ssfn_bbox(&(ctx->font), (char *)&str, &w, &h, NULL, NULL);
				if (ctx->terminal.cx + w >= ctx->fb_modes[ctx->current_mode].width) {
					ctx->terminal.cx = 0;
					ctx->terminal.cy += h;
				}
				font_write(ctx, (char *)&str, ctx->terminal.cx, ctx->terminal.cy);
				ctx->terminal.cx += w;
				break;
			}
		}
		s++;
	}
}

void terminal_setcur(struct ui_context *ui, uint32_t x, uint32_t y)
{
	ui->terminal.cx = x;
	ui->terminal.cy = y;
}
