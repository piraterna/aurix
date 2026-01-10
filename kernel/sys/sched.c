/*********************************************************************************/
/* Module Name:  sched.c */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/*********************************************************************************/

#include <sys/sched.h>
#include <mm/heap.h>
#include <debug/log.h>
#include <string.h>
#include <mm/vmm.h>

#define SCHED_DEFAULT_SLICE 10

void sched_init()
{
	if (cpu_get_current()->thread_count != 0) {
		warn("tried to init scheduler when current CPU already has threads.\n");
		return;
	}

	// just hope that the thread list is empty :^)
	cpu_get_current()->thread_count = 0;
	cpu_get_current()->thread_list = NULL;
}

pcb *proc_create()
{
	pcb *proc = (pcb *)kmalloc(sizeof(pcb));
	if (!proc) {
		error("Failed to create process: Failed to allocate memory!\n");
		return NULL;
	}

	proc->pid = (uint64_t)proc; // for now
	proc->pm = create_pagemap();
	proc->threads = NULL; // dont create a thread yet
	debug("Created new process with PID=%ld, pagemap=%p\n", proc->pid,
		  proc->pm);
	return proc;
}

void proc_destroy(pcb *proc)
{
	destroy_pagemap(proc->pm);
	// TODO: clear up threads
	info("Destroyed process with PID=%ld\n", proc->pid);
	kfree(proc);
}