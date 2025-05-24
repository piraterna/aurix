/*********************************************************************************/
/* Module Name:  handoff.c                                                       */
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

#include <arch/cpu/gdt.h>
#include <arch/cpu/idt.h>
#include <proto/aurix.h>
#include <mm/vmm.h>
#include <stdint.h>

struct gdt {
	struct gdt_descriptor null;
	struct gdt_descriptor kcode;
	struct gdt_descriptor kdata;
	struct gdt_descriptor ucode;
	struct gdt_descriptor udata;
	struct tss_descriptor tss;
};

void aurix_arch_handoff(void *kernel_entry, pagetable *pm, void *stack, uint32_t stack_size, struct aurix_parameters *parameters)
{
	struct tss tss = {
		.iomap_base = sizeof(struct tss),
	};

	struct gdt gdt = {
		.tss = {
			.gdt = {
				.base_low = ((uintptr_t)&tss) & 0xFFFF,
				.base_mid = (((uintptr_t)&tss) >> 16) & 0xFF,
				.base_high = (((uintptr_t)&tss) >> 24) & 0xFF,
				.access = 0b10001001
			},
			.base_high = (((uintptr_t)&tss) >> 32) & 0xFFFF
		},
	};

	struct gdtr gdtr = {
		.base = (uint64_t)&gdt,
		.limit = sizeof(gdt) - 1
	};

	gdt_set_entry(&gdt.null, 0, 0, 0, 0);
	gdt_set_entry(&gdt.kcode, 0, 0, 0x9a, 0xaf);
	gdt_set_entry(&gdt.kdata, 0, 0, 0x92, 0xcf);
	gdt_set_entry(&gdt.ucode, 0, 0, 0xfa, 0xaf);
	gdt_set_entry(&gdt.udata, 0, 0, 0xf2, 0xcf);

	struct idtr idtr = {
		.base = (uint64_t)0,
		.limit = 0
	};

	__asm__ volatile(
					"cli\n"
					"lgdt %[gdtr]\n"
					"ltr %[tss]\n"
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
					"movq %%rax, %%gs\n"

					"lidt %[idt]\n"

					"movq %[pml4], %%cr3\n"
					"movq %[stack], %%rsp\n"
					"movq %[params], %%rdi\n"
					"callq *%[entry]\n"
					:: [gdtr]"g"(gdtr), [tss]"r"((uint16_t)__builtin_offsetof(struct gdt, tss)),
						[idt]"g"(idtr),
						[pml4]"r"(pm), [stack]"r"(stack + stack_size),
						[entry]"r"(kernel_entry), [params]"d"(parameters) : "rax", "memory");
}