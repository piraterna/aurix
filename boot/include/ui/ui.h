/*********************************************************************************/
/* Module Name:  ui.h                                                            */
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

#ifndef _UI_UI_H
#define _UI_UI_H

#include <ui/framebuffer.h>
#include <ui/terminal.h>
#include <ui/font.h>

// this is so hacky... but it works
#ifdef FONT_IMPLEMENTATION
#include <lib/string.h>
#include <mm/mman.h>

#define SSFN_memcmp memcmp
#define SSFN_memset memset
#define SSFN_realloc mem_realloc
#define SSFN_free mem_free
#define SSFN_IMPLEMENTATION
#endif
#include <ui/ssfn.h>

#include <stdint.h>

struct ui_context {
	uint32_t *fb_addr;
	struct fb_mode *fb_modes;
	int total_modes;
	int current_mode;
	int ui;

	struct terminal terminal;

	ssfn_t font;
	ssfn_buf_t font_buf;
	char *font_file;
};

void ui_init();

#endif /* _UI_UI_H */