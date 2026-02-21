/*********************************************************************************/
/* Module Name:  panic.c */
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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#include <sys/panic.h>

#include <arch/apic/apic.h>
#include <arch/cpu/cpu.h>
#include <cpu/trace.h>
#include <mm/vmm.h>
#include <sys/sched.h>
#include <util/kprintf.h>
#include <nanoprintf.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdarg.h>

#define KPANIC_COLOR 1

#if KPANIC_COLOR
#define KPANIC_RED "\033[1;91m"
#define KPANIC_RED_BG "\033[1;37;41m"
#define KPANIC_DIM "\033[2m"
#define KPANIC_BOLD "\033[1m"
#define KPANIC_RESET "\033[0m"
#else
#define KPANIC_RED ""
#define KPANIC_RED_BG ""
#define KPANIC_DIM ""
#define KPANIC_BOLD ""
#define KPANIC_RESET ""
#endif

extern struct cpu cpuinfo[];
extern size_t cpu_count;

static atomic_bool panicking = ATOMIC_VAR_INIT(false);

static void panic_stop_other_cpus(uint8_t this_cpu)
{
	for (size_t i = 0; i < cpu_count; i++) {
		if (i == this_cpu)
			continue;
		lapic_write(0x310, cpuinfo[i].id << 24);
		lapic_write(0x300, 0xff);
	}
}

static void panic_print_symbol(uintptr_t addr)
{
	const char *name = NULL;
	uintptr_t sym = 0;
	if (trace_lookup_symbol(addr, &name, &sym) && name) {
		kprintf("0x%.16llx <%s+0x%llx>", (unsigned long long)addr, name,
				(unsigned long long)(addr - sym));
		return;
	}

	if (addr)
		kprintf("0x%.16llx", (unsigned long long)addr);
	else
		kprintf("(null)");
}

static void panic_backtrace(uintptr_t pm_phys, uintptr_t rbp, uint16_t depth)
{
	pagetable *pm = pm_phys ? (pagetable *)pm_phys : NULL;
	uintptr_t prev = 0;

	for (uint16_t i = 0; i < depth; i++) {
		if (!rbp)
			break;
		if (rbp & 0x7)
			break;
		if (vget_phys(pm, rbp) == 0 || vget_phys(pm, rbp + sizeof(void *)) == 0)
			break;

		uintptr_t *rbp_ptr = (uintptr_t *)rbp;
		uintptr_t ret = rbp_ptr[1];
		if (!ret)
			break;

		kprintf("  #%u ", i);
		panic_print_symbol(ret);
		kprintf("\n");

		uintptr_t next = rbp_ptr[0];
		if (next == rbp || next == prev)
			break;
		prev = rbp;
		rbp = next;
	}
}

void kpanic(const struct interrupt_frame *frame, const char *reason)
{
	cpu_disable_interrupts();
	_log_force_unlock();

	if (atomic_exchange(&panicking, true)) {
		kprintf("\n" KPANIC_RED_BG " KERNEL PANIC " KPANIC_RESET " %s\n",
				reason ? reason : "panic");
		for (;;)
			cpu_halt();
	}

	struct cpu *cpu = cpu_get_current();
	uint8_t cpu_id = cpu ? cpu->id : 0;

	panic_stop_other_cpus(cpu_id);

	tcb *t = thread_current();
	pcb *p = t ? t->process : NULL;
	uint32_t tid = t ? t->tid : 0;
	uint32_t pid = p ? p->pid : 0;
	const char *pname = (p && p->name) ? p->name : NULL;
	bool show_task = (p && p->pid != 0);

	kprintf(
		"\n" KPANIC_RED_BG
		"====================== KERNEL PANIC ======================" KPANIC_RESET
		"\n");
	kprintf(KPANIC_BOLD "reason" KPANIC_RESET ": %s\n",
			reason ? reason : "panic");
	if (show_task) {
		kprintf(KPANIC_BOLD "where " KPANIC_RESET ": cpu=%u pid=%u tid=%u",
				cpu_id, pid, tid);
		if (pname)
			kprintf(" module=%s", pname);
		kprintf("\n");
	} else {
		kprintf(KPANIC_BOLD "where " KPANIC_RESET ": cpu=%u\n", cpu_id);
	}

	if (frame) {
		kprintf(KPANIC_BOLD "fault " KPANIC_RESET ": ");
		panic_print_symbol((uintptr_t)frame->rip);
		kprintf("\n");

		kprintf(KPANIC_DIM "exc" KPANIC_RESET
						   "   : vec=%llu err=0x%llx cr2=0x%llx cr3=0x%llx\n",
				(unsigned long long)frame->vector,
				(unsigned long long)frame->err, (unsigned long long)frame->cr2,
				(unsigned long long)frame->cr3);
		kprintf(KPANIC_DIM
				"regs" KPANIC_RESET
				"  : rax=%016llx rbx=%016llx rcx=%016llx rdx=%016llx\n",
				(unsigned long long)frame->rax, (unsigned long long)frame->rbx,
				(unsigned long long)frame->rcx, (unsigned long long)frame->rdx);
		kprintf("        rdi=%016llx rsi=%016llx rbp=%016llx rsp=%016llx\n",
				(unsigned long long)frame->rdi, (unsigned long long)frame->rsi,
				(unsigned long long)frame->rbp, (unsigned long long)frame->rsp);
		kprintf("        r8 =%016llx r9 =%016llx r10=%016llx r11=%016llx\n",
				(unsigned long long)frame->r8, (unsigned long long)frame->r9,
				(unsigned long long)frame->r10, (unsigned long long)frame->r11);
		kprintf("        r12=%016llx r13=%016llx r14=%016llx r15=%016llx\n",
				(unsigned long long)frame->r12, (unsigned long long)frame->r13,
				(unsigned long long)frame->r14, (unsigned long long)frame->r15);
		kprintf("        rfl=%016llx cs=%016llx  ds=%016llx\n",
				(unsigned long long)frame->rflags,
				(unsigned long long)frame->cs, (unsigned long long)frame->ds);

		kprintf("\n" KPANIC_BOLD "backtrace" KPANIC_RESET ":\n");
		kprintf("  " KPANIC_RED "<fault>" KPANIC_RESET " ");
		panic_print_symbol((uintptr_t)frame->rip);
		kprintf("\n");
		panic_backtrace(frame->cr3, (uintptr_t)frame->rbp, 16);
	} else {
		kprintf("\n" KPANIC_BOLD "backtrace" KPANIC_RESET ":\n");
		stack_trace(16);
	}

	kprintf(
		KPANIC_RED_BG
		"==========================================================" KPANIC_RESET
		"\n");

	for (;;)
		cpu_halt();
	UNREACHABLE();
}

void kpanicf(const struct interrupt_frame *frame, const char *fmt, ...)
{
	char buf[256];
	va_list args;
	va_start(args, fmt);
	(void)npf_vsnprintf(buf, sizeof(buf), fmt ? fmt : "panic", args);
	va_end(args);
	kpanic(frame, buf);
}
