/*********************************************************************************/
/* Module Name:  font.c                                                          */
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

#include <mm/mman.h>
#include <lib/string.h>
#include <lib/assert.h>
#include <vfs/vfs.h>
#define FONT_IMPLEMENTATION
#include <ui/ui.h>
#include <ui/font.h>
#include <print.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>

bool font_init(struct ui_context *ctx, char *font_path, int size)
{
	vfs_read(font_path, &(ctx->font_file));
	if (!ctx->font_file) {
		return false;
	}

	int ssfn_status;

	ssfn_status = ssfn_load(&(ctx->font), (void *)(ctx->font_file));
	if (ssfn_status != SSFN_OK) {
		debug("font_init(): SSFN failed to load font: %s!\n", ssfn_error(ssfn_status));
		goto error;
	}

	ssfn_status = ssfn_select(&(ctx->font), SSFN_FAMILY_ANY, NULL, SSFN_STYLE_REGULAR, size);
	if (ssfn_status != SSFN_OK) {
		debug("font_init(): SSFN failed to select font: %s!\n", ssfn_error(ssfn_status));
		goto error;
	}

	// initialize terminal
	ctx->terminal.font_size = size;
	ctx->terminal.cx = 0;
	ctx->terminal.cy = size;

	return true;

error:
	mem_free(ctx->font_file);
	return false;
}

void font_write(struct ui_context *ctx, char *s, uint32_t cx, uint32_t cy)
{
	ctx->font_buf.x = cx;
	ctx->font_buf.y = cy;
	ssfn_render(&(ctx->font), &(ctx->font_buf), s);
}

void font_free(struct ui_context *ctx)
{
	ssfn_free(&(ctx->font));
	mem_free(ctx->font_file);
}

/*
void font_ttf_init(char *font_path, int initial_size)
{
	vfs_read(font_path, (char **)&font_buf);
	if (!font_buf) {
		debug("Font not loaded, returning...\n");
		return;
	}
}

void font_psf2_init()
{
}
*/