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

#include <arch/apic/apic.h>
#include <arch/cpu/cpu.h>
#include <arch/cpu/idt.h>
#include <arch/cpu/irq.h>
#include <cpu/trace.h>
#include <sys/panic.h>
#include <sys/sched.h>
#include <aurix.h>
#include <stdint.h>
#include <stddef.h>

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

extern struct cpu cpuinfo[];
extern size_t cpu_count;

void idt_init()
{
	for (int v = 0; v < 32; v++) {
		idt_set_desc(&idt[v], (uint64_t)isr_stubs[v], IDT_TRAP, 0);
	}
	for (int v = 32; v < 256; v++) {
		idt_set_desc(&idt[v], (uint64_t)isr_stubs[v], IDT_INTERRUPT, 0);
	}

	__asm__ volatile("lidt %0" ::"m"(idtr));
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
		kpanic(&frame, exception_str[frame.vector]);
	} else if (frame.vector < 0x80) {
		uint8_t irq = frame.vector - 0x20;
		irq_dispatch(irq);
		apic_send_eoi();
		if (irq == 0) {
			sched_tick();
		}
	} else if (frame.vector == 0xfe) {
		apic_send_eoi();
		sched_yield();
	} else if (frame.vector == 0xff) {
		// shutdown
		cpu_halt();
		UNREACHABLE();
	} else {
		warn("Unhandled interrupt %u\n", frame.vector);
	}
}
