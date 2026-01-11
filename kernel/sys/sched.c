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

#include <sys/sched.h>
#include <sys/spinlock.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <debug/log.h>
#include <string.h>

#define SCHED_DEFAULT_SLICE 10

static uint32_t next_pid = 1;
static uint32_t next_tid = 1;

static struct cpu *sched_pick_best_cpu(void)
{
	if (cpu_count == 0)
		return cpu_get_current();

	struct cpu *best = NULL;
	uint64_t best_count = UINT64_MAX;

	for (size_t i = 0; i < cpu_count; i++) {
		struct cpu *cpu = &cpuinfo[i];

		spinlock_acquire(&cpu->sched_lock);
		uint64_t count = cpu->thread_count;
		spinlock_release(&cpu->sched_lock);

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

	spinlock_acquire(&cpu->sched_lock);

	thread->cpu_next = cpu->thread_list;
	cpu->thread_list = thread;
	cpu->thread_count++;
	thread->cpu = cpu;

	spinlock_release(&cpu->sched_lock);
}

static void cpu_remove_thread(struct cpu *cpu, tcb *thread)
{
	if (!cpu || !thread)
		return;

	spinlock_acquire(&cpu->sched_lock);

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

	spinlock_release(&cpu->sched_lock);
}

void sched_init(void)
{
	struct cpu *cpu = cpu_get_current();

	spinlock_init(&cpu->sched_lock);
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

	proc->pid = MAKE_ID(PID_KIND_NORMAL_PROCESS, next_pid++);
	proc->pm = create_pagemap();
	proc->vctx = vinit(proc->pm, 0x1000);
	proc->threads = NULL;

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
	thread->tid = MAKE_ID(TID_KIND_NORMAL_THREAD, next_tid++);
	thread->process = proc;
	thread->time_slice = SCHED_DEFAULT_SLICE;
	thread->frame = (struct interrupt_frame){ 0 };
	thread->frame.rip = (uint64_t)entry;

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

	debug("Created TID=%u PID=%u CPU=%u\n", thread->tid, proc->pid, cpu->id);

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
