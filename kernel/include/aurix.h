/*********************************************************************************/
/* Module Name:  aurix.h */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy */
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

#ifndef _AURIX_H
#define _AURIX_H

#include <config.h>
#include <debug/log.h>
#include <stdint.h>
#include <mm/vmm.h>

extern uintptr_t hhdm_offset;
extern vctx_t *kvctx;

#define PHYS_TO_VIRT(addr) ((uintptr_t)(addr) + hhdm_offset)
#define VIRT_TO_PHYS(addr) ((uintptr_t)(addr) - hhdm_offset)

#ifndef UNREACHABLE
#define UNREACHABLE() __builtin_unreachable()
#endif

static inline void hexdump(const void *data, size_t size)
{
	const unsigned char *p = (const unsigned char *)data;

	for (size_t i = 0; i < size; i += 16) {
		kprintf("%08zx  ", i);

		for (size_t j = 0; j < 16; j++) {
			if (i + j < size)
				kprintf("%02x ", p[i + j]);
			else
				kprintf("   ");

			if (j == 7)
				kprintf(" ");
		}

		kprintf(" |");

		for (size_t j = 0; j < 16 && i + j < size; j++) {
			unsigned char c = p[i + j];
			if (c >= 32 && c <= 126)
				kprintf("%c", c);
			else
				kprintf(".");
		}

		kprintf("|\n");
	}
}

#endif /* _AURIX_H */