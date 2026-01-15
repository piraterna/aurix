/*********************************************************************************/
/* Module Name:  sched.c                                                          */
/* Project:      AurixOS                                                          */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                             */
/*                                                                               */
/* This source is subject to the MIT License.                                     */
/* See License.txt in the root of this repository.                                */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE  */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#include <boot/axprot.h>
#include <sys/sched.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <debug/log.h>
#include <string.h>
#include <arch/sys/irqlock.h>
#include <lib/align.h>
#include <arch/cpu/switch.h>
#include <aurix.h>

#ifdef __x86_64__
#include <platform/time/pit.h>
#endif

#define SCHED_DEFAULT_SLICE 10

static uint32_t next_pid = 1;
static uint32_t next_tid = 1;

extern char _start_text[];
extern char _end_text[];
extern char _start_rodata[];
extern char _end_rodata[];
extern char _start_data[];
extern char _end_data[];

static struct cpu *sched_pick_best_cpu(void)
{
	if (cpu_count == 0)
		return cpu_get_current();

	struct cpu *best = NULL;
	uint64_t best_count = UINT64_MAX;

	for (size_t i = 0; i < cpu_count; i++) {
		struct cpu *cpu = &cpuinfo[i];

		irqlock_acquire(&cpu->sched_lock);
		uint64_t count = cpu->thread_count;
		irqlock_release(&cpu->sched_lock);

		if (count < best_count) {
			best = cpu;
			best_count = count;
		}
	}

	return best;
}

static void cpu_add_thread(struct cpu *cpu, tcb *thread)
{
	if (!cpu || !thread) {
		warn("NULL arg (cpu=%p, thread=%p)\n", cpu, thread);
		return;
	}

	irqlock_acquire(&cpu->sched_lock);

	thread->cpu_next = cpu->thread_list;
	cpu->thread_list = thread;
	cpu->thread_count++;
	thread->cpu = cpu;

	debug("Created TID=%u PID=%u CPU=%u\n", thread->tid, thread->process->pid,
		  cpu->id);

	irqlock_release(&cpu->sched_lock);
}

static void cpu_remove_thread(struct cpu *cpu, tcb *thread)
{
	if (!cpu || !thread)
		return;

	irqlock_acquire(&cpu->sched_lock);

	tcb **link = &cpu->thread_list;
	while (*link) {
		if (*link == thread) {
			*link = thread->cpu_next;
			cpu->thread_count--;

			debug("TID=%u removed from CPU=%u (count=%lu)\n", thread->tid,
				  cpu->id, cpu->thread_count);
			break;
		}
		link = &(*link)->cpu_next;
	}

	irqlock_release(&cpu->sched_lock);
}

static tcb *cpu_pick_next_thread(struct cpu *cpu, tcb *current)
{
	if (!cpu || !cpu->thread_list)
		return NULL;

	tcb *next = current ? current->cpu_next : cpu->thread_list;

	if (!next)
		next = cpu->thread_list;

	return next;
}

void sched_tick(void)
{
	struct cpu *cpu = cpu_get_current();
	if (!cpu || !cpu->thread_list)
		return;

	tcb *current = cpu->thread_list;
	if (!current)
		return;

	debug("CPU=%u TID=%u tick: remaining slice=%u\n", cpu->id, current->tid,
		  current->time_slice);

	if (current->time_slice > 0)
		current->time_slice--;

	if (current->time_slice == 0) {
		debug("CPU=%u TID=%u slice expired, yielding CPU...\n", cpu->id,
			  current->tid);
		sched_yield();
	}
}

void sched_yield(void)
{
	struct cpu *cpu = cpu_get_current();
	if (!cpu || !cpu->thread_list)
		return;

	tcb *current = cpu->thread_list;
	if (!current)
		return;

	current->time_slice = SCHED_DEFAULT_SLICE;
	trace("CPU=%u TID=%u yielding CPU\n", cpu->id, current->tid);

	irqlock_acquire(&cpu->sched_lock);

	tcb *next = cpu_pick_next_thread(cpu, current);
	struct kthread next_kthread = (struct kthread){ 0, 0, (uint64_t)next->rsp };
	if (!next || next == current) {
		debug("CPU=%u TID=%u: only one thread, no switch needed\n", cpu->id,
			  current->tid);
		irqlock_release(&cpu->sched_lock);
		switch_task(NULL, &next_kthread);
		return;
	}

	tcb *prev = cpu->thread_list;
	while (prev->cpu_next && prev->cpu_next != next)
		prev = prev->cpu_next;

	if (prev->cpu_next == next) {
		prev->cpu_next = next->cpu_next;
		next->cpu_next = cpu->thread_list;
		cpu->thread_list = next;
	}

	debug("CPU=%u switched from TID=%u to TID=%u\n", cpu->id, current->tid,
		  next->tid);

	tcb *t = cpu->thread_list;
	debug("CPU=%u thread list: \n", cpu->id);
	while (t) {
		debug("%u -> \n", t->tid);
		t = t->cpu_next;
	}

	irqlock_release(&cpu->sched_lock);
	struct kthread prev_kthread = (struct kthread){ 0, 0, (uint64_t)prev->rsp };
	switch_task(&prev_kthread, &next_kthread);
}

void sched_init(void)
{
	struct cpu *cpu = cpu_get_current();

	irqlock_init(&cpu->sched_lock);
	cpu->thread_list = NULL;
	cpu->thread_count = 0;

	info("Initialized on CPU=%u\n", cpu->id);
}

pcb *proc_create(void)
{
	pcb *proc = kmalloc(sizeof(pcb));
	if (!proc) {
		error("proc_create OOM\n");
		return NULL;
	}

	memset(proc, 0, sizeof(pcb));

	proc->pid = next_pid++;
	proc->pm = create_pagemap();
	proc->vctx = vinit(proc->pm, 0x1000);
	proc->threads = NULL;

	uintptr_t kvirt = 0xffffffff80000000ULL;
	uintptr_t kphys = boot_params->kernel_addr;

	uint64_t text_start = ALIGN_DOWN((uintptr_t)_start_text, PAGE_SIZE);
	uint64_t text_end = ALIGN_UP((uintptr_t)_end_text, PAGE_SIZE);
	map_pages(proc->pm, text_start, text_start - kvirt + kphys,
			  text_end - text_start, VMM_PRESENT);

	uint64_t rodata_start = ALIGN_DOWN((uintptr_t)_start_rodata, PAGE_SIZE);
	uint64_t rodata_end = ALIGN_UP((uintptr_t)_end_rodata, PAGE_SIZE);
	map_pages(proc->pm, rodata_start, rodata_start - kvirt + kphys,
			  rodata_end - rodata_start, VMM_PRESENT | VMM_NX);

	uint64_t data_start = ALIGN_DOWN((uintptr_t)_start_data, PAGE_SIZE);
	uint64_t data_end = ALIGN_UP((uintptr_t)_end_data, PAGE_SIZE);
	map_pages(proc->pm, data_start, data_start - kvirt + kphys,
			  data_end - data_start, VMM_PRESENT | VMM_WRITABLE | VMM_NX);

	debug("Created process PID=%u (pm=%p)\n", proc->pid, proc->pm);

	return proc;
}

void proc_destroy(pcb *proc)
{
	if (!proc)
		return;

	tcb *t = proc->threads;
	while (t) {
		tcb *next = t->proc_next;
		thread_destroy(t);
		t = next;
	}

	vdestroy(proc->vctx);
	destroy_pagemap(proc->pm);
	kfree(proc);

	debug("Destroyed process, PID=%u\n", proc->pid);
}

tcb *thread_create(pcb *proc, void (*entry)(void))
{
	if (!proc) {
		warn("NULL process\n");
		return NULL;
	}

	tcb *thread = kmalloc(sizeof(tcb));
	if (!thread) {
		error("Failed to allocate memory for new thread\n");
		return NULL;
	}

	memset(thread, 0, sizeof(tcb));

	thread->magic = TCB_MAGIC_ALIVE;
	thread->tid = next_tid++;
	thread->process = proc;
	thread->time_slice = SCHED_DEFAULT_SLICE;

	uint64_t *stack_base = valloc(kvctx, DIV_ROUND_UP(STACK_SIZE, PAGE_SIZE),
								  VMM_PRESENT | VMM_WRITABLE | VMM_NX);
	if (!stack_base) {
		kfree(thread);
		return NULL;
	}

	map_page(NULL, (uintptr_t)stack_base, (uintptr_t)stack_base,
			 VMM_PRESENT | VMM_WRITABLE | VMM_NX);

	uint64_t *rsp = (uint64_t *)((uint8_t *)stack_base + STACK_SIZE);

	/* Initial stack for new thread to work with switch_task */
	*--rsp = 0; // rbx
	*--rsp = 0; // rbp
	*--rsp = 0; // r12
	*--rsp = 0; // r13
	*--rsp = 0; // r14
	*--rsp = 0; // r15
	*--rsp = 0x202; // rflags
	*--rsp = (uint64_t)entry; // RIP (return address)

	thread->rsp = rsp;

	if (!proc->threads) {
		proc->threads = thread;
	} else {
		tcb *cur = proc->threads;
		while (cur->proc_next)
			cur = cur->proc_next;
		cur->proc_next = thread;
	}

	struct cpu *cpu = sched_pick_best_cpu();
	cpu_add_thread(cpu, thread);

	return thread;
}

void thread_destroy(tcb *thread)
{
	if (!thread)
		return;

	if (thread->magic != TCB_MAGIC_ALIVE) {
		warn("Invalid TCB %p\n", thread);
		return;
	}

	if (thread->cpu) {
		cpu_remove_thread(thread->cpu, thread);
		thread->cpu = NULL;
	}

	if (thread->process) {
		tcb **link = &thread->process->threads;
		while (*link) {
			if (*link == thread) {
				*link = thread->proc_next;
				break;
			}
			link = &(*link)->proc_next;
		}
	}

	thread->magic = TCB_MAGIC_DEAD;
	thread->proc_next = (tcb *)0xDEADDEAD;
	thread->cpu_next = (tcb *)0xDEADDEAD;
	thread->process = NULL;

	kfree(thread);
}
