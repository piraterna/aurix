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

// TODO: Remove this if statement once I fix stb_truetype compilation
#if 0

#include <mm/mman.h>
#include <arch/lib/math.h>
#include <lib/string.h>
#include <lib/assert.h>
#include <vfs/vfs.h>
#include <print.h>

__attribute__((used)) static volatile int _fltused = 0;

#define STB_TRUETYPE_IMPLEMENTATION

#define STBTT_ifloor(x) _ifloor(x)
#define STBTT_iceil(x) _iceil(x)
#define STBTT_sqrt(x) _sqrt(x)
#define STBTT_pow(x, y) _pow(x, y)
#define STBTT_fmod(x, y) _fmod(x, y)
#define STBTT_cos(x) _cos(x)
#define STBTT_acos(x) _acos(x)
#define STBTT_fabs(x) __builtin_fabs(x)

#define STBTT_malloc(x, u) ((void)(u), mem_alloc(x))
#define STBTT_free(x, u) ((void)(u), mem_free(x))

#define STBTT_assert(x) assert(x, #x)
#define STBTT_strlen(x) strlen(x)

#define STBTT_memcpy memcpy
#define STBTT_memset memset

#include "stb_truetype.h"

unsigned char *font_buf = NULL;
stbtt_fontinfo font_info;
float font_scale;
int font_ascent, font_descent;
int font_linegap;
int font_size;

void font_init(char *font_path, int initial_size)
{
	vfs_read(font_path, &font_buf);
	if (!font_buf) {
		debug("Font not loaded, returning...\n");
		return;
	}

	font_size = initial_size;

	stbtt_InitFont(&font_info, &font_buf, 0);
	font_scale = stbtt_ScaleForPixelHeight(&font_info, font_size);
	stbtt_GetFontVMetrics(&font_info, &font_ascent, &font_descent, &font_linegap);
	font_ascent *= font_scale;
	font_descent *= font_scale;
}

#endif