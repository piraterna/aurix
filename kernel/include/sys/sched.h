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
#include <mm/vmm.h>

#define STACK_SIZE 4096 * 4 // ~16KB

struct pcb;
struct tcb;

#define TCB_MAGIC_ALIVE 0x544352414C495645ULL // "TCRALIVE"
#define TCB_MAGIC_DEAD 0x544352444541444ULL // "TCRDEAD"

typedef struct tcb {
	uint64_t magic;
	uint32_t tid;

	uint32_t time_slice;

	struct pcb *process;
	struct cpu *cpu;

	uint64_t *rsp;

	struct tcb *proc_next;
	struct tcb *cpu_next;

} tcb;

typedef struct pcb {
	uint32_t pid;
	pagetable *pm;
	vctx_t *vctx;
	struct tcb *threads;
} pcb;

void sched_init(void);
void sched_tick(void);
void sched_yield(void);

pcb *proc_create(void);
void proc_destroy(pcb *proc);

tcb *thread_create(pcb *proc, void (*entry)(void));
void thread_destroy(tcb *thread);
void thread_exit(tcb *thread);
tcb *thread_current(void);

#endif // _SYS_SCHED_H