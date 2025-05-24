/*********************************************************************************/
/* Module Name:  framebuffer.h                                                   */
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

#ifndef _UI_FRAMEBUFFER_H
#define _UI_FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>

enum fb_format {
	FB_RGBA = 0,
	FB_BGRA = 1
};

struct fb_mode {
	uint32_t width;
	uint32_t height;
	uint8_t bpp;
	uint32_t pitch;
	int format;
};

bool get_framebuffer(uint32_t **fb_addr, struct fb_mode **available_modes, int *total_modes, int *current_mode_index);

#endif /* _UI_FRAMEBUFFER_H */