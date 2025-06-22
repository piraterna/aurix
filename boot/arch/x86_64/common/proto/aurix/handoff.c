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
#include <print.h>
#include <stdint.h>

void aurix_arch_handoff(void *kernel_entry, pagetable *pm, void *stack, uint32_t stack_size, struct aurix_parameters *parameters)
{
	struct gdt_descriptor gdt[5];
	
	gdt_set_entry(&gdt[0], 0, 0, 0, 0);
	gdt_set_entry(&gdt[1], 0, 0, 0x9a, 0xaf);
	gdt_set_entry(&gdt[2], 0, 0, 0x92, 0xcf);
	gdt_set_entry(&gdt[3], 0, 0, 0xfa, 0xaf);
	gdt_set_entry(&gdt[4], 0, 0, 0xf2, 0xcf);

	struct gdtr gdtr = {
		.base = (uint64_t)&gdt,
		.limit = sizeof(gdt) - 1
	};

	struct idtr idtr = {
		.base = (uint64_t)0,
		.limit = 0
	};

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

					"lidt %[idt]\n"

					"movq %[pml4], %%cr3\n"
					"movq %[stack], %%rsp\n"
					"movq %[params], %%rdi\n"
					"movq %[entry], %%rsi\n"

					// rsi = kernel entry point
					// rdi = kernel parameters
					"xor %%rax, %%rax\n"
					"xor %%rbx, %%rbx\n"
					"xor %%rcx, %%rcx\n"
					"xor %%rdx, %%rdx\n"
					//"xor %%rdi, %%rdi\n"
					//"xor %%rsi, %%rsi\n"
					"xor %%r8, %%r8\n"
					"xor %%r9, %%r9\n"
					"xor %%r10, %%r10\n"
					"xor %%r11, %%r11\n"
					"xor %%r12, %%r12\n"
					"xor %%r13, %%r13\n"
					"xor %%r14, %%r14\n"
					"xor %%r15, %%r15\n"

					"callq *%%rsi\n"
					:: [gdtr]"g"(gdtr), [idt]"g"(idtr),
						[pml4]"r"(pm), [stack]"r"(stack + stack_size),
						[entry]"r"(kernel_entry), [params]"d"(parameters) : "rax", "memory");
}
