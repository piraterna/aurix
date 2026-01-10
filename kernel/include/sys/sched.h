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

#ifndef _SYS_SCHED_H
#define _SYS_SCHED_H

#include <arch/mm/paging.h>
#include <arch/cpu/cpu.h>
#include <stdint.h>

struct pcb;
struct tcb;

typedef struct tcb {
	uint64_t tid;
	struct interrupt_frame frame;
	struct pcb *process;
	struct tcb *next;
	uint32_t time_slice;
} tcb;

typedef struct pcb {
	uint64_t pid;
	pagetable *pm;
	struct tcb *threads;
} pcb;

void sched_init(void);
pcb *proc_create(void);
void proc_destroy(pcb *proc);

#endif // _SYS_SCHED_H