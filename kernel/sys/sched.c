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

void sched_init()
{
	if (cpu_get_current()->thread_count != 0) {
		warn("tried to init scheduler when current CPU already has threads.\n");
		return;
	}

	// just hope that the thread list is empty :^)
	// TODO: clear on every CPU
	cpu_get_current()->thread_count = 0;
	cpu_get_current()->thread_list = NULL;
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

	destroy_pagemap(proc->pm);

	info("Destroyed process with PID=%ld\n", proc->pid);
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

	debug("Created new thread TID=%u (kind=%u, seq=%u), parent PID=%u\n",
		  thread->tid, ID_KIND(thread->tid), ID_SEQ(thread->tid), proc->pid);

	return thread;
}

void thread_destroy(tcb *thread)
{
	if (!thread)
		return;

	/* Check magic first — do NOT trust heap yet */
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
