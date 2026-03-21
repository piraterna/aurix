/*********************************************************************************/
/* Module Name:  gdt.c */
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

#include <arch/cpu/gdt.h>
#include <arch/cpu/cpu.h>
#include <config.h>
#include <stdint.h>
#include <string.h>

static struct gdt_descriptor gdt[CONFIG_CPU_MAX_COUNT][7];
struct tss gdt_tss[CONFIG_CPU_MAX_COUNT];

void gdt_init()
{
	uint32_t cpu = cpu_get_current_id();
	if (cpu >= CONFIG_CPU_MAX_COUNT)
		cpu = 0;

	memset(gdt[cpu], 0, sizeof(gdt[cpu]));
	memset(&gdt_tss[cpu], 0, sizeof(gdt_tss[cpu]));

	gdt_set_entry(&gdt[cpu][0], 0, 0, 0, 0);
	gdt_set_entry(&gdt[cpu][1], 0, 0xfffff, 0x9a,
				  0x0a); //KCS //TODO: Set accessed bit?
	gdt_set_entry(&gdt[cpu][2], 0, 0xfffff, 0x92, 0x0c); //KDS
	gdt_set_entry(&gdt[cpu][3], 0, 0xfffff, 0xf2,
				  0x0c); //UDS
	gdt_set_entry(&gdt[cpu][4], 0, 0xfffff, 0xfa, 0x0a); //UCS
	gdt_set_entry(&gdt[cpu][5], ((uint64_t)&gdt_tss[cpu]) & 0xffffffff,
				  sizeof(struct tss) - 1, 0x89,
				  0); // TSS low
	*(uint64_t *)&gdt[cpu][6] = (uint64_t)&gdt_tss[cpu] >> 32; // TSS high

	gdt_tss[cpu].iopb_offset = sizeof(struct tss);

	struct gdtr gdtr = { .base = (uintptr_t)&gdt[cpu][0],
						 .limit = sizeof(gdt[cpu]) - 1 };

	__asm__ volatile("lgdt %[gdtr]\n"
					 "pushq $0x08\n"
					 "lea 1f(%%rip), %%rax\n"
					 "pushq %%rax\n"
					 "lretq\n"
					 "1:\n"
					 "movq $0x10, %%rax\n"
					 "movw %%ax, %%ds\n"
					 "movw %%ax, %%es\n"
					 "movw %%ax, %%ss\n"
					 "movw %%ax, %%fs\n"
					 "movw %%ax, %%gs\n" ::[gdtr] "g"(gdtr)
					 : "memory");

	uint16_t tss_index = 5 * sizeof(struct gdt_descriptor);

	__asm__ volatile("ltr %0" ::"d"(tss_index));
}

void gdt_set_kernel_stack(uint64_t rsp0)
{
	uint32_t cpu = cpu_get_current_id();
	if (cpu >= CONFIG_CPU_MAX_COUNT)
		cpu = 0;

	gdt_tss[cpu].rsp_ring[0] = rsp0;
}

void gdt_set_entry(struct gdt_descriptor *entry, uint32_t base, uint32_t limit,
				   uint8_t access, uint8_t flags)
{
	entry->limit_low = limit & 0xffff;
	entry->base_low = base & 0xffff;
	entry->base_mid = (base >> 16) & 0xff;
	entry->access = access;
	entry->limit_flags = ((limit >> 16) & 0xf) | (flags << 4);
	entry->base_high = (base >> 24) & 0xff;
}
