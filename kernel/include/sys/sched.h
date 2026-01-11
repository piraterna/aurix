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

struct pcb;
struct tcb;

#define TCB_MAGIC_ALIVE 0x544352414C495645ULL // "TCRALIVE"
#define TCB_MAGIC_DEAD 0x544352444541444ULL // "TCRDEAD"

#define PID_KIND_NORMAL_PROCESS 1
#define TID_KIND_NORMAL_THREAD 1

#define ID_KIND_SHIFT 24
#define ID_SEQ_MASK 0x00FFFFFFu

#define MAKE_ID(kind, seq)                            \
	((uint32_t)(((uint32_t)(kind) << ID_KIND_SHIFT) | \
				((uint32_t)(seq) & ID_SEQ_MASK)))

#define ID_KIND(id) ((uint8_t)((id) >> ID_KIND_SHIFT))
#define ID_SEQ(id) ((uint32_t)((id) & ID_SEQ_MASK))

typedef struct tcb {
	uint64_t magic;
	uint32_t tid;

	struct interrupt_frame frame;
	uint32_t time_slice;

	struct pcb *process;
	struct cpu *cpu;

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

pcb *proc_create(void);
void proc_destroy(pcb *proc);

tcb *thread_create(pcb *proc, void (*entry)(void));
void thread_destroy(tcb *thread);

static inline int pid_valid(uint32_t pid)
{
	return ID_KIND(pid) == PID_KIND_NORMAL_PROCESS && ID_SEQ(pid) != 0;
}

static inline int tid_valid(uint32_t tid)
{
	return ID_KIND(tid) == TID_KIND_NORMAL_THREAD && ID_SEQ(tid) != 0;
}

#endif // _SYS_SCHED_H