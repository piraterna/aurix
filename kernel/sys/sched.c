/*********************************************************************************/
/* Module Name:  sched.h */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#include <sys/sched.h>
#include <mm/heap.h>
#include <debug/log.h>
#include <string.h>
#include <mm/vmm.h>

#define SCHED_DEFAULT_SLICE 10

static uint32_t next_pid = 1;
static uint32_t next_tid = 1;

static struct cpu *sched_pick_best_cpu(void)
{
	if (cpu_count == 0)
		return cpu_get_current();

	struct cpu *best = &cpuinfo[0];

	for (size_t i = 1; i < cpu_count; i++) {
		if (cpuinfo[i].thread_count < best->thread_count)
			best = &cpuinfo[i];
	}

	return best;
}

static void cpu_add_thread(struct cpu *cpu, tcb *thread)
{
	if (!cpu || !thread) {
		warn("NULL cpu or thread (cpu=%p, thread=%p)\n", cpu, thread);
		return;
	}

	spinlock_acquire(&cpu->sched_lock);

	thread->next = cpu->thread_list;
	cpu->thread_list = thread;
	cpu->thread_count++;
	thread->cpu = cpu;

	debug("CPU=%u TID=%u (thread_count=%lu)\n", cpu->id, thread->tid,
		  cpu->thread_count);

	spinlock_release(&cpu->sched_lock);
}

static void cpu_remove_thread(struct cpu *cpu, tcb *thread)
{
	if (!cpu || !thread) {
		warn("NULL cpu or thread (cpu=%p, thread=%p)\n", cpu, thread);
		return;
	}

	spinlock_acquire(&cpu->sched_lock);

	tcb **link = &cpu->thread_list;
	while (*link) {
		if (*link == thread) {
			*link = thread->next;
			cpu->thread_count--;

			debug("CPU=%u TID=%u (thread_count=%lu)\n", cpu->id, thread->tid,
				  cpu->thread_count);

			spinlock_release(&cpu->sched_lock);
			return;
		}
		link = &(*link)->next;
	}

	spinlock_release(&cpu->sched_lock);

	warn("TID=%u not found on CPU=%u\n", thread->tid, cpu->id);
}

void sched_init()
{
	struct cpu *cpu = cpu_get_current();

	spinlock_init(&cpu->sched_lock);
	cpu->thread_count = 0;
	cpu->thread_list = NULL;

	info("Scheduler initialized on CPU=%u\n", cpu->id);
}

pcb *proc_create()
{
	pcb *proc = kmalloc(sizeof(pcb));
	if (!proc) {
		error("Failed to create process: Failed to allocate memory!\n");
		return NULL;
	}

	memset(proc, 0, sizeof(pcb));

	proc->pid = MAKE_ID(PID_KIND_NORMAL_PROCESS, next_pid++);
	proc->pm = create_pagemap();
	proc->vctx = vinit(proc->pm, 0x1000);
	proc->threads = NULL;

	debug("Created new process PID=%u (kind=%u, seq=%u), pagemap=%p\n",
		  proc->pid, ID_KIND(proc->pid), ID_SEQ(proc->pid), proc->pm);

	return proc;
}

void proc_destroy(pcb *proc)
{
	if (!proc)
		return;

	tcb *curr = proc->threads;
	while (curr) {
		tcb *next = curr->next;
		thread_destroy(curr);
		curr = next;
	}

	vdestroy(proc->vctx);
	destroy_pagemap(proc->pm);

	info("Destroyed process with PID=%u\n", proc->pid);
	kfree(proc);
}

tcb *thread_create(pcb *proc, void (*entry)(void))
{
	if (!proc) {
		warn("NULL process passed\n");
		return NULL;
	}
	(void)proc;
	(void)entry;

	tcb *thread = (tcb *)kmalloc(sizeof(tcb));
	if (!thread) {
		error("Failed to create thread: Failed to allocate memory!\n");
		return NULL;
	}
	memset(thread, 0, sizeof(tcb));

	thread->magic = TCB_MAGIC_ALIVE;
	thread->process = proc;
	thread->frame = (struct interrupt_frame){ 0 };
	thread->next = NULL;
	thread->tid = MAKE_ID(TID_KIND_NORMAL_THREAD, next_tid++);
	thread->time_slice = SCHED_DEFAULT_SLICE;

	// TODO: setup full frame, stack and stuff
	thread->frame.rip = (uint64_t)
		entry; // this just hopes the entry function is an address in the parent procs pagemap.

	// Append to the process's thread list
	if (!proc->threads) {
		proc->threads = thread;
	} else {
		tcb *curr = proc->threads;
		while (curr->next) {
			curr = curr->next;
		}
		curr->next = thread;
	}

	struct cpu *target_cpu = sched_pick_best_cpu();
	cpu_add_thread(target_cpu, thread);

	debug("Created new thread TID=%u (kind=%u, seq=%u), parent PID=%u\n",
		  thread->tid, ID_KIND(thread->tid), ID_SEQ(thread->tid), proc->pid);

	return thread;
}

void thread_destroy(tcb *thread)
{
	if (!thread)
		return;

	if (thread->magic != TCB_MAGIC_ALIVE) {
		warn(
			"Tried to destroy a thread that is already destroyed or corrupt (%p)\n",
			thread);
		return;
	}

	pcb *proc = thread->process;

	// Unlink the thread from process list
	if (proc) {
		tcb **link = &proc->threads;
		while (*link) {
			if (*link == thread) {
				*link = thread->next;
				break;
			}
			link = &(*link)->next;
		}
	}

	// Poison fields (kill it basically, incase the memory doesnt get overwritten)
	thread->magic = TCB_MAGIC_DEAD;
	thread->next = (tcb *)0xDEADDEAD;
	thread->process = NULL;

	info("Destroyed thread with TID=%ld\n", thread->tid);
	kfree(thread);
}
