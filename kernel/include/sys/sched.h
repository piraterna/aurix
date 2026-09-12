/*********************************************************************************/
/* Module Name:  sched.h */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy */
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
#include <arch/cpu/switch.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <mm/vmm.h>
#include <stdatomic.h>
#include <sys/spinlock.h>

#define STACK_SIZE 4096 * 8
#define USER_STACK_SIZE (1024 * 1024)

struct pcb;
struct tcb;

#define TCB_MAGIC_ALIVE 0x544352414C495645ULL
#define TCB_MAGIC_DEAD 0x544352444541444ULL

#define PROC_MAX_FDS 256

struct fileio;

typedef struct tcb {
	uint64_t magic;
	uint32_t tid;
	bool user;

	uint32_t time_slice;

	struct pcb *process;
	struct cpu *cpu;

	struct kthread kthread;

	struct tcb *proc_next;
	struct tcb *cpu_next;

	bool joinable;
	int exit_code;
	atomic_bool finished;

	atomic_bool kill_pending;
	int kill_code;
} tcb;

typedef struct pcb {
	uint32_t pid;
	pagetable *pm;
	vctx_t *vctx;
	struct tcb *threads;
	struct pcb *proc_next;
	spinlock_t fd_lock;
	spinlock_t thread_lock;
	struct fileio *fds[PROC_MAX_FDS];
	const char *name;
	char *cwd;
	mode_t umask;
	uid_t uid;
	gid_t gid;
	uid_t euid;
	gid_t egid;
	uint32_t parent_pid;
	int exit_code;
	bool exited;
	char *image_elf;
	size_t image_size;
	uintptr_t image_phys_base;
	uintptr_t image_load_base;
	uintptr_t image_link_base;
	size_t image_exec_size;
	uintptr_t user_stack_base;
	size_t user_stack_size;
	uintptr_t user_rsp;
	atomic_bool kill_pending;
	int kill_code;
	atomic_uint thread_count;
	atomic_bool reaped;
} pcb;

void sched_init(void);
void sched_tick(void);
void sched_yield(void);
void sched_enable(void);
void sched_disable(void);
bool sched_is_enabled(void);

pcb *proc_create(void);
void proc_destroy(pcb *proc);
bool proc_has_threads(uint32_t pid);
pcb *proc_get_by_pid(uint32_t pid);
int proc_kill(pcb *proc, int code);

tcb *thread_create(pcb *proc, void (*entry)(void));
tcb *thread_create_user(pcb *proc, void (*entry)(void));
tcb *thread_clone_user(pcb *proc, tcb *parent);
void thread_enqueue(tcb *thread);
void thread_destroy(tcb *thread);
void thread_exit(tcb *thread, int code);
tcb *thread_current(void);
tcb *thread_get_by_tid(uint32_t tid);
int thread_wait(tcb *thread);

#endif // SYS_SCHED_H