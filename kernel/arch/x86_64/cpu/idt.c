/*********************************************************************************/
/* Module Name:  idt.c */
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

#include <arch/cpu/cpu.h>
#include <arch/cpu/idt.h>
#include <cpu/trace.h>
#include <debug/log.h>
#include <stdint.h>

#define IDT_TRAP 0xF
#define IDT_INTERRUPT 0xE

const char *exception_str[32] = {
	"division by zero",
	"debug",
	"nmi",
	"breakpoint",
	"overflow",
	"bound range exceeded",
	"invalid opcode",
	"device not available",
	"double fault",
	"reserved", // coprocessor segment overrun
	"invalid tss",
	"segment not present",
	"stack-segment fault",
	"general protection fault",
	"page fault",
	"reserved",
	"x87 exception",
	"alignment check",
	"machine check",
	"simd exception",
	"virtualization exception",
	"control protection",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"hypervisor injection",
	"vmm communication",
	"security",
	"reserved",
};

__attribute__((aligned(16))) struct idt_descriptor idt[256];

struct idtr idtr = { .limit =
						 (uint16_t)((sizeof(struct idt_descriptor) * 256) - 1),
					 .base = (uint64_t)&idt[0] };

extern void *isr_stubs[256];
void *irq_handlers[16] = { 0 };

void idt_init()
{
	for (int v = 0; v < 32; v++) {
		idt_set_desc(&idt[v], (uint64_t)isr_stubs[v], IDT_TRAP, 0);
	}
	for (int v = 32; v < 256; v++) {
		idt_set_desc(&idt[v], (uint64_t)isr_stubs[v], IDT_INTERRUPT, 0);
	}

	__asm__ volatile("lidt %0" ::"m"(idtr));
	__asm__ volatile("sti");
}

void idt_set_desc(struct idt_descriptor *desc, uint64_t offset, uint8_t type,
				  uint8_t dpl)
{
	desc->base_low = offset & 0xFFFF;
	desc->codeseg = 0x08;
	desc->ist = 0;
	desc->flags = (1 << 7) | (dpl << 5) | (type);
	desc->base_mid = (offset >> 16) & 0xFFFF;
	desc->base_high = (offset >> 32) & 0xFFFFFFFF;
	desc->reserved = 0;
}

void isr_common_handler(struct interrupt_frame frame)
{
	if (frame.vector < 0x20) {
		// TODO: get cpu which triggered the exception
		error(
			"panic(cpu %u): Kernel trap at 0x%.16llx, type %u=%s, registers:\n",
			1, frame.rip, frame.vector, exception_str[frame.vector]);
		error(
			"rax: 0x%.16llx, rbx: 0x%.16llx, rcx: 0x%.16llx, rdx: 0x%.16llx\n",
			frame.rax, frame.rbx, frame.rcx, frame.rdx);
		error(
			"rbp: 0x%.16llx, rdi: 0x%.16llx, rsi: 0x%.16llx, rsp: 0x%.16llx\n",
			frame.rbp, frame.rdi, frame.rsi, frame.rsp);
		error(
			"r8:  0x%.16llx, r9:  0x%.16llx, r10: 0x%.16llx, r11: 0x%.16llx\n",
			frame.r8, frame.r9, frame.r10, frame.r11);
		error(
			"r12: 0x%.16llx, r13: 0x%.16llx, r14: 0x%.16llx, r15: 0x%.16llx\n",
			frame.r12, frame.r13, frame.r14, frame.r15);
		error(
			"cr0: 0x%.16llx, cr2: 0x%.16llx, cr3: 0x%.16llx, cr4: 0x%.16llx\n",
			frame.cr0, frame.cr2, frame.cr3, frame.cr4);
		error(
			"rfl: 0x%.16llx, rip: 0x%.16llx, cs:  0x%.16llx, ds:  0x%.16llx\n",
			frame.rflags, frame.rip, frame.cs, frame.ds);
		error("err: 0x%.16llx\n", frame.err);

		error("Backtrace (cpu %u):\n", 1);
		stack_trace(16);
	} else {
		warn("Unhandled interrupt %u\n", frame.vector);
	}
	__asm__ volatile("cli;hlt");
}