/*********************************************************************************/
/* Module Name:  idt.c */
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

#include <arch/apic/apic.h>
#include <arch/cpu/cpu.h>
#include <arch/cpu/idt.h>
#include <arch/cpu/gdt.h>
#include <arch/cpu/irq.h>
#include <cpu/trace.h>
#include <sys/panic.h>
#include <sys/sched.h>
#include <aurix.h>
#include <stdint.h>
#include <stddef.h>
#include <user/syscall.h>

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

	// syscall handler
	idt_set_desc(&idt[0x80], (uint64_t)isr_stubs[0x80], IDT_TRAP,
				 3); // DPL=3 for user access

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

static void isr_handle_user_exception(const struct interrupt_frame *frame)
{
	tcb *current = thread_current();
	if (!current || !current->process || current->process->pid == 0) {
		kpanic(frame, exception_str[frame->vector]);
	}

	if (frame->vector == 14) {
		error(
			"exception %s rip=0x%llx cr2=0x%llx err=0x%llx occured in %s (PID=%u, TID=%u)\n",
			exception_str[frame->vector], frame->rip, frame->cr2, frame->err,
			current->process->name ? current->process->name : "<unknown>",
			current->process->pid, current->tid);
	} else {
		error("exception %s (0x%llx) occured in %s (PID=%u, TID=%u)\n",
			  exception_str[frame->vector], frame->rip,
			  current->process->name ? current->process->name : "<unknown>",
			  current->process->pid, current->tid);
	}

	error("!!! fsbase value: 0x%llx\n", rdmsr(0xC0000100));

#if CONFIG_MPANIC_DUMP
	error(

		"regs: rax=%016llx rbx=%016llx rcx=%016llx rdx=%016llx\n",
		(unsigned long long)frame->rax, (unsigned long long)frame->rbx,
		(unsigned long long)frame->rcx, (unsigned long long)frame->rdx);

	error(

		"      rdi=%016llx rsi=%016llx rbp=%016llx rsp=%016llx\n",
		(unsigned long long)frame->rdi, (unsigned long long)frame->rsi,
		(unsigned long long)frame->rbp, (unsigned long long)frame->rsp);

	error(

		"      r8 =%016llx r9 =%016llx r10=%016llx r11=%016llx\n",
		(unsigned long long)frame->r8, (unsigned long long)frame->r9,
		(unsigned long long)frame->r10, (unsigned long long)frame->r11);

	error(

		"      r12=%016llx r13=%016llx r14=%016llx r15=%016llx\n",
		(unsigned long long)frame->r12, (unsigned long long)frame->r13,
		(unsigned long long)frame->r14, (unsigned long long)frame->r15);

	error(

		"      rip=%016llx rfl=%016llx cs =%016llx ss =%016llx\n",
		(unsigned long long)frame->rip, (unsigned long long)frame->rflags,
		(unsigned long long)frame->cs, (unsigned long long)frame->ss);

	error(

		"      vec=%016llx err=%016llx cr2=%016llx cr3=%016llx\n",
		(unsigned long long)frame->vector, (unsigned long long)frame->err,
		(unsigned long long)frame->cr2, (unsigned long long)frame->cr3);
#endif

	panic_dump_to_file(frame, exception_str[frame->vector]);

	thread_exit(current, -1);

	struct cpu *cpu = cpu_get_current();
	tcb *next = cpu ? cpu->thread_list : NULL;

	if (!next) {
		cpu_halt();
		UNREACHABLE();
	}

	gdt_set_kernel_stack(next->kthread.rsp0);
	switch_task(NULL, &next->kthread);
	UNREACHABLE();
}

static void isr_syscall_handler(struct interrupt_frame *frame)
{
	tcb *current = thread_current();
	if (!current || !current->process) {
		kpanic(frame, "syscall from invalid context");
	}

	syscall_args_t args = { .rdi = frame->rdi,
							.id = frame->rax,
							.rsi = frame->rsi,
							.rdx = frame->rdx,
							.r10 = frame->r10,
							.r8 = frame->r8,
							.r9 = frame->r9,
							.rip = frame->rip,
							.rflags = frame->rflags,
							.rsp = frame->rsp };

	if (frame->vector == 0x80) {
		warn("%s called via an int 0x80 syscall! perferably use \"syscall\"\n",
			 syscall_table[args.id].name);
	}

	int64_t ret = syscall_dispatch((uint32_t)args.id, &args);

	frame->rax = ret;
}

void isr_common_handler(struct interrupt_frame frame)
{
	if (frame.vector < 0x20) {
		isr_handle_user_exception(&frame);
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
	} else if (frame.vector == 0x80) {
		isr_syscall_handler(&frame);
	} else {
		warn("Unhandled interrupt %u\n", frame.vector);
	}
}
