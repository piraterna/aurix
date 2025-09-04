/*********************************************************************************/
/* Module Name:  handoff.c */
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
#include <arch/cpu/idt.h>
#include <mm/vmm.h>
#include <print.h>
#include <proto/aurix.h>
#include <stdint.h>

void aurix_arch_handoff(void *kernel_entry, pagetable *pm, void *stack,
						uint32_t stack_size,
						struct aurix_parameters *parameters)
{
	struct gdt_descriptor gdt[5];
	gdt_set_entry(&gdt[0], 0, 0, 0, 0);
	gdt_set_entry(&gdt[1], 0, 0, 0x9a, 0x0a);
	gdt_set_entry(&gdt[2], 0, 0, 0x92, 0x0c);
	gdt_set_entry(&gdt[3], 0, 0, 0xfa, 0x0a);
	gdt_set_entry(&gdt[4], 0, 0, 0xf2, 0x0c);

	struct gdtr gdtr = { .base = (uint64_t)&gdt, .limit = sizeof(gdt) - 1 };
	struct idtr idtr = { .base = 0, .limit = 0 };

	__asm__ volatile(
		"cli\n"
		"cld\n"

		"lgdt %[gdtr]\n"
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

		"lidt %[idtr]\n"

		"movq %[pml4], %%cr3\n"
		"movq %[parameters], %%rdi\n"
		"movq %[entry], %%rsi\n"
		"xor %%rax, %%rax\n"
		"xor %%rbx, %%rbx\n"
		"xor %%rcx, %%rcx\n"
		"xor %%rdx, %%rdx\n"
		"xor %%r8, %%r8\n"
		"xor %%r9, %%r9\n"
		"xor %%r10, %%r10\n"
		"xor %%r11, %%r11\n"
		"xor %%r12, %%r12\n"
		"xor %%r13, %%r13\n"
		"xor %%r14, %%r14\n"
		"xor %%r15, %%r15\n"

		"xor %%rbp, %%rbp\n"
		"callq *%%rsi\n"
		:
		: [gdtr] "m"(gdtr), [idtr] "m"(idtr),
		  [stack_top] "r"((uint64_t)stack + stack_size), [pml4] "r"(pm),
		  [parameters] "r"(parameters), [entry] "r"(kernel_entry)
		: "rax", "memory");
}
