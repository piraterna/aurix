/*********************************************************************************/
/* Module Name:  gdt.h                                                           */
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

#ifndef _ARCH_CPU_GDT_H
#define _ARCH_CPU_GDT_H

#include <stdint.h>

struct gdt_descriptor {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_mid;
	uint8_t access;
	uint8_t limit_high : 4;
	uint8_t flags : 4;
	uint8_t base_high;
} __attribute__((packed));

struct gdtr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

void gdt_init(void);
void gdt_set_entry(struct gdt_descriptor *entry, uint32_t base, uint32_t limit, uint8_t access,
				   uint8_t flags);

#endif /* _ARCH_CPU_GDT_H */