/*********************************************************************************/
/* Module Name:  gdt.c */
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

#include <arch/cpu/gdt.h>
#include <stdint.h>

void gdt_init()
{
	struct gdt_descriptor gdt[5];

	gdt_set_entry(&gdt[0], 0, 0, 0, 0);
	gdt_set_entry(&gdt[1], 0, 0, 0x9a, 0xaf);
	gdt_set_entry(&gdt[2], 0, 0, 0x92, 0xcf);
	gdt_set_entry(&gdt[3], 0, 0, 0xfa, 0xaf);
	gdt_set_entry(&gdt[4], 0, 0, 0xf2, 0xcf);

	struct gdtr gdtr = { .base = (uintptr_t)&gdt, .limit = sizeof(gdt) - 1 };

	__asm__ volatile("lgdt %[gdtr]\n"
					 "pushq $0x08\n"
					 "lea 1f(%%rip), %%rax\n"
					 "pushq %%rax\n"
					 "lretq\n"
					 "1:\n"
					 "movq $0x10, %%rax\n"
					 "movq %%rax, %%ds\n"
					 "movq %%rax, %%es\n"
					 "movq %%rax, %%ss\n"
					 "movq %%rax, %%fs\n"
					 "movq %%rax, %%gs\n" ::[gdtr] "g"(gdtr)
					 : "memory");
}

void gdt_set_entry(struct gdt_descriptor *entry, uint32_t base, uint32_t limit,
				   uint8_t access, uint8_t flags)
{
	entry->limit_low = (limit >> 8) & 0xffff;
	entry->base_low = (base >> 8) & 0xffff;
	entry->base_mid = (base >> 16) & 0xff;
	entry->access = access;
	entry->limit_high = (limit >> 16) & 0xf;
	entry->flags = flags;
	entry->base_high = (base >> 24) & 0xff;
}