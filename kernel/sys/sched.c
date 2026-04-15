/*********************************************************************************/
/* Module Name:  sched.c                                                          */
/* Project:      AurixOS                                                          */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy                                             */
/*                                                                               */
/* This source is subject to the MIT License.                                     */
/* See License.txt in the root of this repository.                                */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE    */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER         */
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
#ifdef __x86_64__
#include <arch/cpu/gdt.h>
#endif
#include <arch/apic/apic.h>
#include <aurix.h>
#include <stdatomic.h>
#include <acpi/madt.h>
#include <vfs/fileio.h>

#ifdef __x86_64__
#include <platform/time/pit.h>
#endif

#define SCHED_DEFAULT_SLICE 10
#define USER_STACK_SIZE (1024 * 1024)

#define PID_MAX 65536u
#define TID_MAX 65536u

static atomic_uint live_proc_count = ATOMIC_VAR_INIT(0);
static atomic_uint live_thread_count = ATOMIC_VAR_INIT(0);

static spinlock_t pid_alloc_lock = { 0 };
static spinlock_t tid_alloc_lock = { 0 };

static uint8_t pid_used[PID_MAX];
static uint8_t tid_used[TID_MAX];

static uint32_t pid_next_hint = 1; /* 0 reserved for kernel */
static uint32_t tid_next_hint = 1; /* 0 reserved */
static bool id_alloc_inited = false;

static atomic_bool sched_enabled = ATOMIC_VAR_INIT(false);

static pcb kernel_proc;
static int kernel_proc_inited = 0;
static tcb idle_threads[CONFIG_CPU_MAX_COUNT];
static bool cpu_sched_inited[CONFIG_CPU_MAX_COUNT] = { false };
static pcb *proc_list = NULL;
static spinlock_t proc_list_lock = { 0 };

extern char _start_text[];
extern char _end_text[];
extern char _start_rodata[];
extern char _end_rodata[];
extern char _start_data[];
extern char _end_data[];
extern void switch_enter_user(void);

#ifdef __x86_64__
static inline void sched_prepare_cpu_stack(const tcb *next)
{
	if (next)
		gdt_set_kernel_stack(next->kthread.rsp0);
}
#else
static inline void sched_prepare_cpu_stack(const tcb *next)
{
	(void)next;
}
#endif

static void sched_id_allocator_init(void)
{
	if (id_alloc_inited)
		return;

	spinlock_init(&pid_alloc_lock);
	spinlock_init(&tid_alloc_lock);

	memset(pid_used, 0, sizeof(pid_used));
	memset(tid_used, 0, sizeof(tid_used));

	pid_used[0] = 1;
	tid_used[0] = 1;

	pid_next_hint = 1;
	tid_next_hint = 1;
	id_alloc_inited = true;
}

static uint32_t sched_alloc_pid(void)
{
	uint32_t pid = UINT32_MAX;

	spinlock_acquire(&pid_alloc_lock);

	for (uint32_t scanned = 0; scanned < (PID_MAX - 1); scanned++) {
		uint32_t cand = pid_next_hint;
		if (cand == 0 || cand >= PID_MAX)
			cand = 1;

		if (!pid_used[cand]) {
			pid_used[cand] = 1;
			pid = cand;
			pid_next_hint = cand + 1;
			if (pid_next_hint >= PID_MAX)
				pid_next_hint = 1;
			break;
		}

		pid_next_hint++;
		if (pid_next_hint >= PID_MAX)
			pid_next_hint = 1;
	}

	spinlock_release(&pid_alloc_lock);
	return pid;
}

static void sched_free_pid(uint32_t pid)
{
	if (pid == 0 || pid >= PID_MAX)
		return;

	spinlock_acquire(&pid_alloc_lock);
	pid_used[pid] = 0;
	if (pid < pid_next_hint)
		pid_next_hint = pid;
	spinlock_release(&pid_alloc_lock);
}

static uint32_t sched_alloc_tid(void)
{
	uint32_t tid = UINT32_MAX;

	spinlock_acquire(&tid_alloc_lock);

	for (uint32_t scanned = 0; scanned < (TID_MAX - 1); scanned++) {
		uint32_t cand = tid_next_hint;
		if (cand == 0 || cand >= TID_MAX)
			cand = 1;

		if (!tid_used[cand]) {
			tid_used[cand] = 1;
			tid = cand;
			tid_next_hint = cand + 1;
			if (tid_next_hint >= TID_MAX)
				tid_next_hint = 1;
			break;
		}

		tid_next_hint++;
		if (tid_next_hint >= TID_MAX)
			tid_next_hint = 1;
	}

	spinlock_release(&tid_alloc_lock);
	return tid;
}

static void sched_free_tid(uint32_t tid)
{
	if (tid == 0 || tid >= TID_MAX)
		return;

	spinlock_acquire(&tid_alloc_lock);
	tid_used[tid] = 0;
	if (tid < tid_next_hint)
		tid_next_hint = tid;
	spinlock_release(&tid_alloc_lock);
}

static inline bool sched_tid_is_managed(uint32_t tid)
{
	return tid > 0 && tid < TID_MAX;
}

static void sched_ipi_cpu(struct cpu *target)
{
	if (!target || target->id == cpu_get_current()->id)
		return;

	lapic_write(0x310, (uint32_t)target->id << 24);
	lapic_write(0x300, (uint32_t)0xfe | (1u << 14));

	while (lapic_read(0x300) & (1u << 12))
		;
}

static struct cpu *sched_pick_best_cpu(void)
{
	if (cpu_count == 1)
		return cpu_get_current();

	struct cpu *best = NULL;
	uint64_t best_count = UINT64_MAX;

	for (size_t i = 0; i < cpu_count; i++) {
		struct cpu *c = &cpuinfo[i];

		if (c->id < CONFIG_CPU_MAX_COUNT && !cpu_sched_inited[c->id])
			continue;

		irqlock_acquire(&c->sched_lock);
		uint64_t count = c->thread_count;
		irqlock_release(&c->sched_lock);

		if (count < best_count ||
			(count == best_count && c == cpu_get_current())) {
			best = c;
			best_count = count;
		}
	}

	return best ? best : cpu_get_current();
}

static void cpu_add_thread(struct cpu *cpu, tcb *thread)
{
	if (!cpu || !thread) {
		warn("NULL arg (cpu=%p, thread=%p)\n", cpu, thread);
		return;
	}

	irqlock_acquire(&cpu->sched_lock);

	thread->cpu_next = NULL;

	if (!cpu->thread_list) {
		cpu->thread_list = thread;
	} else {
		tcb *tail = cpu->thread_list;
		while (tail->cpu_next)
			tail = tail->cpu_next;
		tail->cpu_next = thread;
	}

	cpu->thread_count++;
	thread->cpu = cpu;

	trace("Added TID=%u (owner pid: %u) to -> CPU%u\n", thread->tid,
		  thread->process ? thread->process->pid : UINT32_MAX, cpu->id);

	irqlock_release(&cpu->sched_lock);

	if (atomic_load(&sched_enabled) && cpu->id != cpu_get_current()->id)
		sched_ipi_cpu(cpu);
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
			break;
		}
		link = &(*link)->cpu_next;
	}

	thread->cpu_next = NULL;
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

static void proc_unlink_thread(pcb *proc, tcb *thread)
{
	if (!proc || !thread)
		return;

	tcb **link = &proc->threads;
	while (*link) {
		if (*link == thread) {
			*link = thread->proc_next;
			thread->proc_next = NULL;
			return;
		}
		link = &(*link)->proc_next;
	}
}

static void thread_release_final(tcb *thread)
{
	if (!thread)
		return;

	uint32_t tid = thread->tid;

	thread->magic = TCB_MAGIC_DEAD;
	thread->proc_next = (tcb *)0xDEADDEAD;
	thread->cpu_next = (tcb *)0xDEADDEAD;
	thread->process = NULL;
	thread->cpu = NULL;

	kfree(thread);

	if (sched_tid_is_managed(tid))
		sched_free_tid(tid);

	atomic_fetch_sub(&live_thread_count, 1);
}

void sched_tick(void)
{
	if (!atomic_load(&sched_enabled))
		return;

	struct cpu *cpu = cpu_get_current();
	if (!cpu)
		return;

	irqlock_acquire(&cpu->sched_lock);

	tcb *current = cpu->thread_list;
	if (!current) {
		irqlock_release(&cpu->sched_lock);
		return;
	}

	if (current->time_slice > 0)
		current->time_slice--;

	bool should_yield = (current->time_slice == 0);
	irqlock_release(&cpu->sched_lock);

	if (should_yield)
		sched_yield();
}

void sched_yield(void)
{
	if (!atomic_load(&sched_enabled))
		return;

	struct cpu *cpu = cpu_get_current();
	if (!cpu)
		return;

	irqlock_acquire(&cpu->sched_lock);

	tcb *current = cpu->thread_list;
	if (!current) {
		irqlock_release(&cpu->sched_lock);
		return;
	}

	current->time_slice = SCHED_DEFAULT_SLICE;

	if (!current->cpu_next) {
		irqlock_release(&cpu->sched_lock);
		return;
	}

	tcb *next = current->cpu_next;
	cpu->thread_list = next;

	tcb *tail = next;
	while (tail->cpu_next)
		tail = tail->cpu_next;
	tail->cpu_next = current;
	current->cpu_next = NULL;

	irqlock_release(&cpu->sched_lock);

	sched_prepare_cpu_stack(next);
	switch_task(&current->kthread, &next->kthread);
}

void sched_enable(void)
{
	atomic_store(&sched_enabled, true);

	for (size_t i = 0; i < cpu_count; i++) {
		struct cpu *c = &cpuinfo[i];
		if (c->id != cpu_get_current()->id)
			sched_ipi_cpu(c);
	}
}

void sched_disable(void)
{
	atomic_store(&sched_enabled, false);
}

bool sched_is_enabled(void)
{
	return atomic_load(&sched_enabled);
}

static void idle(void)
{
	for (;;) {
#ifdef __x86_64__
		__asm__ volatile("sti; hlt; cli");
#elif __aarch64__
		__asm__ volatile("wfe");
#endif
		if (atomic_load(&sched_enabled))
			sched_yield();
	}
}

void sched_init(void)
{
	struct cpu *cpu = cpu_get_current();

	sched_id_allocator_init();

	if (get_actual_cpus() == 4 && cpu->id == 3) {
		warn("Known issue running scheduler on a 4-core system: "
			 "CPU%u will not participate in scheduling.\n",
			 cpu->id);
		return;
	}

	irqlock_init(&cpu->sched_lock);
	cpu->thread_list = NULL;
	cpu->thread_count = 0;

	if (!kernel_proc_inited) {
		memset(&kernel_proc, 0, sizeof(kernel_proc));
		kernel_proc.pid = 0;
		kernel_proc.name = strdup("idle");
		kernel_proc.pm = kernel_pm;
		kernel_proc.vctx = kvctx;
		kernel_proc.threads = NULL;
		spinlock_init(&kernel_proc.fd_lock);
		memset(kernel_proc.fds, 0, sizeof(kernel_proc.fds));
		kernel_proc.umask = 0022;
		kernel_proc.uid = 0;
		kernel_proc.gid = 0;
		kernel_proc.euid = 0;
		kernel_proc.egid = 0;
		kernel_proc_inited = 1;
	}

	if (cpu->id < CONFIG_CPU_MAX_COUNT) {
		tcb *idle_tcb = &idle_threads[cpu->id];
		memset(idle_tcb, 0, sizeof(*idle_tcb));
		idle_tcb->magic = TCB_MAGIC_ALIVE;
		idle_tcb->tid = UINT32_MAX - cpu->id;
		idle_tcb->time_slice = SCHED_DEFAULT_SLICE;
		idle_tcb->process = &kernel_proc;
		idle_tcb->cpu = cpu;

		uint64_t *stack_base =
			valloc(kvctx, DIV_ROUND_UP(STACK_SIZE, PAGE_SIZE), VALLOC_RW);
		uint64_t *rsp = (uint64_t *)((uint8_t *)stack_base + STACK_SIZE);
		rsp = (uint64_t *)((uintptr_t)rsp - 8);
		memset(stack_base, 0, STACK_SIZE);

		*--rsp = (uint64_t)idle;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0x202;

		idle_tcb->kthread.rsp0 = (uint64_t)stack_base + STACK_SIZE;
		idle_tcb->kthread.rsp = (uint64_t)rsp;
		idle_tcb->kthread.cr3 = (uint64_t)kernel_pm;
		sched_prepare_cpu_stack(idle_tcb);

		cpu->thread_list = idle_tcb;
		cpu->thread_count = 1;

		cpu_sched_inited[cpu->id] = true;
	}

	trace("Scheduler initialized on CPU=%u\n", cpu->id);
}

pcb *proc_create(void)
{
	pcb *proc = kmalloc(sizeof(pcb));
	if (!proc) {
		error("proc_create OOM\n");
		return NULL;
	}

	memset(proc, 0, sizeof(pcb));

	proc->pid = sched_alloc_pid();
	if (proc->pid == UINT32_MAX) {
		kfree(proc);
		error("proc_create: out of PIDs\n");
		return NULL;
	}

	atomic_fetch_add(&live_proc_count, 1);

	proc->pm = create_pagemap();
	if (!proc->pm) {
		sched_free_pid(proc->pid);
		atomic_fetch_sub(&live_proc_count, 1);
		kfree(proc);
		error("proc_create: create_pagemap failed\n");
		return NULL;
	}

	proc->vctx = vinit(proc->pm, 0x1000);
	if (!proc->vctx) {
		destroy_pagemap(proc->pm);
		sched_free_pid(proc->pid);
		atomic_fetch_sub(&live_proc_count, 1);
		kfree(proc);
		error("proc_create: vinit failed\n");
		return NULL;
	}

	proc->threads = NULL;
	proc->proc_next = NULL;
	spinlock_init(&proc->fd_lock);
	memset(proc->fds, 0, sizeof(proc->fds));
	proc->cwd = strdup("/");
	proc->umask = 0022;
	proc->uid = 0;
	proc->gid = 0;
	proc->euid = 0;
	proc->egid = 0;
	proc->parent_pid = 0;
	proc->exit_code = 0;
	proc->exited = false;

	proc->fds[0] = open("/dev/stdin", O_RDONLY, 0);
	if (!proc->fds[0])
		warn("proc_create: PID=%u failed to open /dev/stdin\n", proc->pid);

	proc->fds[1] = open("/dev/stdout", O_WRONLY, 0);
	if (!proc->fds[1])
		warn("proc_create: PID=%u failed to open /dev/stdout\n", proc->pid);

	proc->fds[2] = open("/dev/stderr", O_WRONLY, 0);
	if (!proc->fds[2])
		warn("proc_create: PID=%u failed to open /dev/stderr\n", proc->pid);

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

	trace("Created process PID=%u (pm=%p)\n", proc->pid, proc->pm);

	spinlock_acquire(&proc_list_lock);
	proc->proc_next = proc_list;
	proc_list = proc;
	spinlock_release(&proc_list_lock);

	return proc;
}

void proc_destroy(pcb *proc)
{
	if (!proc)
		return;

	uint32_t pid = proc->pid;

	while (proc->threads)
		thread_destroy(proc->threads);

	spinlock_acquire(&proc->fd_lock);
	for (size_t fd = 0; fd < PROC_MAX_FDS; fd++) {
		struct fileio *f = proc->fds[fd];
		proc->fds[fd] = NULL;
		if (f) {
			spinlock_release(&proc->fd_lock);
			close(f);
			spinlock_acquire(&proc->fd_lock);
		}
	}
	spinlock_release(&proc->fd_lock);

	vdestroy(proc->vctx);
	destroy_pagemap(proc->pm);

	if (proc->name)
		kfree((void *)proc->name);
	if (proc->cwd)
		kfree(proc->cwd);

	spinlock_acquire(&proc_list_lock);
	pcb **link = &proc_list;
	while (*link) {
		if (*link == proc) {
			*link = proc->proc_next;
			break;
		}
		link = &(*link)->proc_next;
	}
	spinlock_release(&proc_list_lock);

	kfree(proc);
	sched_free_pid(pid);
	atomic_fetch_sub(&live_proc_count, 1);

	trace("Destroyed process, PID=%u\n", pid);
}

static tcb *thread_create_internal(pcb *proc, void (*entry)(void),
								   bool user_mode)
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
	thread->tid = sched_alloc_tid();
	if (thread->tid == UINT32_MAX) {
		kfree(thread);
		error("Failed to allocate TID for new thread\n");
		return NULL;
	}

	atomic_fetch_add(&live_thread_count, 1);
	thread->user = user_mode;
	thread->process = proc;
	thread->time_slice = SCHED_DEFAULT_SLICE;
	thread->joinable = false;
	atomic_store(&thread->finished, false);

	uint64_t *stack_base =
		valloc(kvctx, DIV_ROUND_UP(STACK_SIZE, PAGE_SIZE), VALLOC_RW);
	if (!stack_base) {
		sched_free_tid(thread->tid);
		atomic_fetch_sub(&live_thread_count, 1);
		kfree(thread);
		return NULL;
	}

	uint64_t *rsp = (uint64_t *)((uint8_t *)stack_base + STACK_SIZE);
	rsp = (uint64_t *)((uintptr_t)rsp - 8);
	memset(stack_base, 0, STACK_SIZE);
	thread->kthread.rsp0 = (uint64_t)stack_base + STACK_SIZE;

	if (!user_mode) {
		*--rsp = (uint64_t)entry;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0x202;
	} else {
		if (!proc->user_rsp || !proc->user_stack_base ||
			proc->user_stack_size == 0) {
			error("user stack not initialized for PID=%u\n", proc->pid);
			vfree(kvctx, stack_base);
			sched_free_tid(thread->tid);
			atomic_fetch_sub(&live_thread_count, 1);
			kfree(thread);
			return NULL;
		}

		uint64_t user_rsp = proc->user_rsp;
		uintptr_t user_stack_end =
			proc->user_stack_base + proc->user_stack_size;

		if (user_rsp < proc->user_stack_base || user_rsp > user_stack_end) {
			error("user rsp out of range for PID=%u (rsp=%p)\n", proc->pid,
				  (void *)user_rsp);
			vfree(kvctx, stack_base);
			sched_free_tid(thread->tid);
			atomic_fetch_sub(&live_thread_count, 1);
			kfree(thread);
			return NULL;
		}

		*--rsp = 0x1b;
		*--rsp = user_rsp;
		*--rsp = 0x202;
		*--rsp = 0x23;
		*--rsp = (uint64_t)entry;

		*--rsp = (uint64_t)switch_enter_user;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0;
		*--rsp = 0x202;
	}

	thread->kthread.rsp = (uint64_t)rsp;
	thread->kthread.cr3 = (uint64_t)proc->pm;

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

tcb *thread_create(pcb *proc, void (*entry)(void))
{
	return thread_create_internal(proc, entry, false);
}

tcb *thread_create_user(pcb *proc, void (*entry)(void))
{
	return thread_create_internal(proc, entry, true);
}

void thread_enqueue(tcb *thread)
{
	if (!thread)
		return;

	struct cpu *cpu = sched_pick_best_cpu();
	cpu_add_thread(cpu, thread);
}

tcb *thread_clone_user(pcb *proc, tcb *parent)
{
	if (!proc || !parent) {
		warn("thread_clone_user: invalid args\n");
		return NULL;
	}

	tcb *thread = kmalloc(sizeof(tcb));
	if (!thread) {
		error("Failed to allocate memory for cloned thread\n");
		return NULL;
	}

	memset(thread, 0, sizeof(tcb));

	thread->magic = TCB_MAGIC_ALIVE;
	thread->tid = sched_alloc_tid();
	if (thread->tid == UINT32_MAX) {
		kfree(thread);
		error("Failed to allocate TID for cloned thread\n");
		return NULL;
	}

	atomic_fetch_add(&live_thread_count, 1);
	thread->user = true;
	thread->process = proc;
	thread->time_slice = SCHED_DEFAULT_SLICE;
	thread->joinable = parent->joinable;
	thread->kthread.fs_base = parent->kthread.fs_base;
	atomic_store(&thread->finished, false);

	uint64_t *stack_base =
		valloc(kvctx, DIV_ROUND_UP(STACK_SIZE, PAGE_SIZE), VALLOC_RW);
	if (!stack_base) {
		sched_free_tid(thread->tid);
		atomic_fetch_sub(&live_thread_count, 1);
		kfree(thread);
		return NULL;
	}

	thread->kthread.rsp0 = (uint64_t)stack_base + STACK_SIZE;
	thread->kthread.cr3 = (uint64_t)proc->pm;

	if (!proc->threads) {
		proc->threads = thread;
	} else {
		tcb *cur = proc->threads;
		while (cur->proc_next)
			cur = cur->proc_next;
		cur->proc_next = thread;
	}

	return thread;
}

void thread_destroy(tcb *thread)
{
	if (!thread)
		return;

	if (thread->magic != TCB_MAGIC_ALIVE && !atomic_load(&thread->finished)) {
		warn("Invalid TCB %p\n", thread);
		return;
	}

	if (thread->cpu) {
		cpu_remove_thread(thread->cpu, thread);
		thread->cpu = NULL;
	}

	if (thread->process) {
		proc_unlink_thread(thread->process, thread);
		thread->process = NULL;
	}

	atomic_store(&thread->finished, true);
	thread_release_final(thread);
}

void thread_exit(tcb *thread, int code)
{
	if (!thread)
		thread = thread_current();
	if (!thread)
		return;

	if (thread->magic != TCB_MAGIC_ALIVE) {
		warn("Invalid thread %p in thread_exit\n", thread);
		return;
	}

	struct cpu *cpu = thread->cpu;
	pcb *proc = thread->process;
	tcb *next = NULL;

	debug("Thread TID=%u exiting\n", thread->tid);

	if (cpu) {
		irqlock_acquire(&cpu->sched_lock);

		next = thread->cpu_next;
		if (!next || next == thread)
			next = (cpu->thread_list != thread) ? cpu->thread_list : NULL;

		tcb **link = &cpu->thread_list;
		while (*link) {
			if (*link == thread) {
				*link = thread->cpu_next;
				cpu->thread_count--;
				break;
			}
			link = &(*link)->cpu_next;
		}

		thread->cpu = NULL;
		thread->cpu_next = NULL;

		irqlock_release(&cpu->sched_lock);
	}

	if (proc) {
		proc_unlink_thread(proc, thread);

		if (!proc->threads) {
			proc->exit_code = code;
			proc->exited = true;
		}

		thread->process = NULL;
	}

	thread->exit_code = code;
	thread->magic = TCB_MAGIC_DEAD;
	atomic_store(&thread->finished, true);

	if (!next) {
		while (1)
			cpu_halt();
	}

	sched_prepare_cpu_stack(next);

	if (!thread->joinable) {
		struct kthread dead_ctx = thread->kthread;
		thread_release_final(thread);
		switch_task(&dead_ctx, &next->kthread);
		__builtin_unreachable();
	}

	struct kthread dead_ctx = thread->kthread;
	switch_task(&dead_ctx, &next->kthread);
	__builtin_unreachable();
}

tcb *thread_current(void)
{
	struct cpu *cpu = cpu_get_current();
	if (!cpu)
		return NULL;

	return cpu->thread_list;
}

tcb *thread_get_by_tid(uint32_t tid)
{
	if (tid == UINT32_MAX)
		return NULL;

	for (size_t cpu_idx = 0; cpu_idx < cpu_count; cpu_idx++) {
		struct cpu *cpu = &cpuinfo[cpu_idx];
		if (!cpu)
			continue;

		irqlock_acquire(&cpu->sched_lock);

		tcb *t = cpu->thread_list;
		while (t) {
			if (t->tid == tid) {
				irqlock_release(&cpu->sched_lock);
				return t;
			}
			t = t->cpu_next;
		}

		irqlock_release(&cpu->sched_lock);
	}

	return NULL;
}

bool proc_has_threads(uint32_t pid)
{
	if (pid == 0 || pid == UINT32_MAX)
		return false;

	for (size_t cpu_idx = 0; cpu_idx < cpu_count; cpu_idx++) {
		struct cpu *cpu = &cpuinfo[cpu_idx];
		if (!cpu)
			continue;

		irqlock_acquire(&cpu->sched_lock);

		tcb *t = cpu->thread_list;
		while (t) {
			if (t->process && t->process->pid == pid) {
				irqlock_release(&cpu->sched_lock);
				return true;
			}
			t = t->cpu_next;
		}

		irqlock_release(&cpu->sched_lock);
	}

	return false;
}

pcb *proc_get_by_pid(uint32_t pid)
{
	spinlock_acquire(&proc_list_lock);

	pcb *p = proc_list;
	while (p) {
		if (p->pid == pid) {
			spinlock_release(&proc_list_lock);
			return p;
		}
		p = p->proc_next;
	}

	spinlock_release(&proc_list_lock);
	return NULL;
}

int thread_wait(tcb *thread)
{
	if (!thread) {
		error("thread_wait: NULL thread\n");
		return -1;
	}

	thread->joinable = true;

	while (!atomic_load(&thread->finished))
		sched_yield();

	int code = thread->exit_code;
	thread_release_final(thread);
	return code;
}