/*********************************************************************************/
/* Module Name:  builtin.c */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#include <user/syscall.h>
#include <util/kprintf.h>
#include <sys/sched.h>
#include <stdarg.h>
#include <vfs/fileio.h>
#include <vfs/vfs.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <lib/string.h>
#include <lib/align.h>
#include <sys/errno.h>
#include <loader/module.h>
#include <time/time.h>

#if defined(__x86_64__)
#include <arch/cpu/cpu.h>
#endif

#include <loader/elf.h>

#define FD_RESERVED_STDIN 0
#define FD_STDOUT 1
#define FD_FIRST_DYNAMIC 2

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4

#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANON 0x20
#define MAP_ANONYMOUS MAP_ANON
#define MAP_GROWSDOWN 0x100
#define MAP_DENYWRITE 0x800
#define MAP_EXECUTABLE 0x1000
#define MAP_LOCKED 0x2000
#define MAP_NORESERVE 0x4000
#define MAP_POPULATE 0x8000
#define MAP_NONBLOCK 0x10000
#define MAP_STACK 0x20000
#define MAP_HUGETLB 0x40000
#define MAP_SYNC 0x80000
#define MAP_FIXED_NOREPLACE 0x100000

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

#if defined(__x86_64__)
#define MSR_IA32_FS_BASE 0xC0000100
#endif

static struct pcb *syscall_current_process(void)
{
	struct tcb *current = thread_current();
	if (!current)
		return NULL;
	return current->process;
}

static struct fileio *proc_fd_lookup_locked(struct pcb *proc, int fd)
{
	if (!proc || fd <= FD_RESERVED_STDIN || fd >= PROC_MAX_FDS)
		return NULL;
	return proc->fds[fd];
}

static int proc_fd_alloc_locked(struct pcb *proc, struct fileio *file)
{
	if (!proc || !file)
		return -EINVAL;

	for (int fd = FD_FIRST_DYNAMIC; fd < PROC_MAX_FDS; fd++) {
		if (!proc->fds[fd]) {
			proc->fds[fd] = file;
			return fd;
		}
	}

	return -EMFILE;
}

int64_t sys_exit(const syscall_args_t *args)
{
	int64_t code = (int64_t)args->rdi;
	info("TID=%u (%s, userspace=%s) exited with code %lld\n",
		 thread_current()->tid,
		 thread_current()->process->name ? thread_current()->process->name :
										   "unknown",
		 thread_current()->user ? "yes" : "no", code);
	thread_exit(thread_current(), (int)code);
	return 0;
}

int64_t sys_open(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	int flags = (int)args->rsi;
	mode_t mode = (mode_t)args->rdx;

	if (!path) {
		error("open(): invalid path (%s)\n", ERRNO_NAME(EFAULT));
		return -EFAULT;
	}

	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -EINVAL;

	struct fileio *f = open(path, flags, mode);
	if (!f) {
		error("open(): failed to open %s (%s)\n", path, ERRNO_NAME(ENOENT));
		return -ENOENT;
	}

	spinlock_acquire(&proc->fd_lock);
	int fd = proc_fd_alloc_locked(proc, f);
	spinlock_release(&proc->fd_lock);
	if (fd < 0) {
		close(f);
		return fd;
	}

	trace("open(): opened %s with flags %d and mode %o\n", path, flags, mode);
	return fd;
}

int64_t sys_read(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	struct pcb *proc = syscall_current_process();
	if (!proc) {
		return -EINVAL;
	}

	spinlock_acquire(&proc->fd_lock);
	struct fileio *f = proc_fd_lookup_locked(proc, fd);
	if (!f) {
		spinlock_release(&proc->fd_lock);
		error("read(): invalid file descriptor (%s)\n", ERRNO_NAME(EBADF));
		return -EBADF;
	}
	fio_retain(f);
	spinlock_release(&proc->fd_lock);

	void *buf = (void *)args->rsi;
	size_t count = (size_t)args->rdx;
	if (!buf && count) {
		close(f);
		error("read(): invalid buffer (%s)\n", ERRNO_NAME(EFAULT));
		return -EFAULT;
	}

	int64_t bytes = (int64_t)read(f, count, buf);
	close(f);
	return bytes;
}

int64_t sys_write(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	struct pcb *proc = syscall_current_process();
	if (!proc) {
		return -EINVAL;
	}

	spinlock_acquire(&proc->fd_lock);
	struct fileio *f = proc_fd_lookup_locked(proc, fd);
	if (!f) {
		spinlock_release(&proc->fd_lock);
		error("write(): invalid file descriptor (%s)\n", ERRNO_NAME(EBADF));
		return -EBADF;
	}
	fio_retain(f);
	spinlock_release(&proc->fd_lock);

	const void *buf = (const void *)args->rsi;
	size_t count = (size_t)args->rdx;
	if (!buf && count) {
		close(f);
		error("write(): invalid buffer (%s)\n", ERRNO_NAME(EFAULT));
		return -EFAULT;
	}

	int r = write(f, (void *)buf, count);
	close(f);

	if (r < 0) {
		error("write(): failed to write to file descriptor %d (%s)\n", fd,
			  ERRNO_NAME(-r));
		return r;
	}
	return r;
}

int64_t sys_close(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	if (fd <= FD_STDOUT || fd >= PROC_MAX_FDS) {
		error("close(): invalid file descriptor (%s)\n", ERRNO_NAME(EBADF));
		return -EBADF;
	}

	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -EINVAL;

	spinlock_acquire(&proc->fd_lock);
	struct fileio *f = proc->fds[fd];
	if (!f) {
		spinlock_release(&proc->fd_lock);
		error("close(): invalid file descriptor (%s)\n", ERRNO_NAME(EBADF));
		return -EBADF;
	}
	proc->fds[fd] = NULL;
	spinlock_release(&proc->fd_lock);

	int r = close(f);
	if (r < 0) {
		error("close(): failed to close file descriptor %d (%s)\n", fd,
			  ERRNO_NAME(-r));
		return r;
	}

	return 0;
}

int64_t sys_mount(const syscall_args_t *args)
{
	const char *source = (const char *)args->rdi;
	const char *target = (const char *)args->rsi;
	const char *fstype = (const char *)args->rdx;
	uint64_t flags = args->r10;
	void *data = (void *)args->r8;

	if (!target || !fstype) {
		return -EFAULT;
	}

	if (flags != 0) {
		return -EINVAL;
	}

	(void)source;

	struct vfs *mounted = vfs_mount(NULL, fstype, (char *)target, data);
	if (!mounted) {
		return -EINVAL;
	}

	return 0;
}

int64_t sys_ioctl(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	int request = (int)args->rsi;
	void *arg = (void *)args->rdx;
	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -EINVAL;

	spinlock_acquire(&proc->fd_lock);
	struct fileio *f = proc_fd_lookup_locked(proc, fd);
	if (!f || !f->private) {
		spinlock_release(&proc->fd_lock);
		return -EBADF;
	}
	fio_retain(f);
	spinlock_release(&proc->fd_lock);

	int ret = vfs_ioctl((struct vnode *)f->private, request, arg);
	close(f);
	if (ret == -1) {
		return -ENOTTY;
	}

	return ret;
}

int64_t sys_load_module(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;

	if (!path) {
		return -EFAULT;
	}

	struct fileio *f = open(path, O_RDONLY, 0);
	if (!f) {
		return -ENOENT;
	}

	if (f->size == 0) {
		close(f);
		return -EINVAL;
	}

	size_t image_size = f->size;

	void *image = kmalloc(image_size);
	if (!image) {
		close(f);
		return -ENOMEM;
	}

	size_t bytes = read(f, image_size, image);
	close(f);

	if (bytes != image_size) {
		kfree(image);
		return -EIO;
	}

	if (!module_load_image(image, (uint32_t)image_size)) {
		kfree(image);
		return -EINVAL;
	}

	return 0;
}

int64_t sys_exec(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	if (!path)
		return -EFAULT;

	struct fileio *f = open(path, O_RDONLY, 0);
	if (!f)
		return -ENOENT;

	if (f->size == 0) {
		close(f);
		return -EINVAL;
	}

	char *buf = (char *)kmalloc(f->size);
	if (!buf) {
		close(f);
		return -ENOMEM;
	}

	if (read(f, f->size, buf) != f->size) {
		close(f);
		kfree(buf);
		return -EIO;
	}

	close(f);

	struct pcb *proc = proc_create();
	if (!proc) {
		kfree(buf);
		return -ENOMEM;
	}

	char *name_copy = kmalloc(strlen(path) + 1);
	if (name_copy)
		strcpy(name_copy, path);
	proc->name = (const char *)name_copy;

	uint64_t image_addr = 0;
	size_t image_size = 0;
	uintptr_t entry = elf_load(buf, &image_addr, &image_size, proc->pm);
	kfree(buf);
	if (entry == 0) {
		proc_destroy(proc);
		return -EINVAL;
	}

	struct tcb *thread = thread_create_user(proc, (void (*)(void))entry);
	if (!thread) {
		proc_destroy(proc);
		return -ENOMEM;
	}

	thread->joinable = true;
	int code = thread_wait(thread);
	proc_destroy(proc);
	return code;
}

int64_t sys_mmap(const syscall_args_t *args)
{
	void *addr = (void *)args->rdi;
	size_t length = (size_t)args->rsi;
	int prot = (int)args->rdx;
	int flags = (int)args->r10;
	int fd = (int)args->r8;
	size_t offset = (size_t)args->r9;

	if (length == 0)
		return -EINVAL;

	if (offset & (PAGE_SIZE - 1))
		return -EINVAL;

	int share_flags = flags & (MAP_SHARED | MAP_PRIVATE);
	if (share_flags == 0 || share_flags == (MAP_SHARED | MAP_PRIVATE))
		return -EINVAL;

	if (flags & ~(MAP_SHARED | MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS |
				  MAP_GROWSDOWN | MAP_DENYWRITE | MAP_EXECUTABLE | MAP_LOCKED |
				  MAP_NORESERVE | MAP_POPULATE | MAP_NONBLOCK | MAP_STACK |
				  MAP_HUGETLB | MAP_SYNC | MAP_FIXED_NOREPLACE))
		return -EINVAL;

	if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
		return -EINVAL;

	if (offset != 0)
		return -EINVAL;

	if (!(flags & MAP_ANONYMOUS)) {
		if (fd < 0)
			return -EBADF;
		return -ENOSYS;
	}

	struct pcb *proc = syscall_current_process();
	if (!proc || !proc->vctx)
		return -EINVAL;

	size_t pages = DIV_ROUND_UP(length, PAGE_SIZE);
	if (!pages)
		return -EINVAL;

	uint64_t vflags = VALLOC_USER;
	if (prot & PROT_READ)
		vflags |= VALLOC_READ;
	if (prot & PROT_WRITE)
		vflags |= VALLOC_WRITE;
	if (prot & PROT_EXEC)
		vflags |= VALLOC_EXEC;

	void *mapped = NULL;
	uintptr_t min_addr = VPM_MIN_ADDR;
	uintptr_t hint = (uintptr_t)addr;

	if (flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) {
		if (!addr)
			return -EINVAL;
		if (!IS_PAGE_ALIGNED(addr))
			return -EINVAL;
		if (hint < min_addr)
			return -EINVAL;
		mapped = vallocatv(proc->vctx, hint, pages, vflags);
	} else if (addr != NULL) {
		if (hint < min_addr)
			hint = min_addr;
		hint = ALIGN_DOWN(hint, PAGE_SIZE);
		mapped = vallocatv(proc->vctx, hint, pages, vflags);
		if (!mapped)
			mapped = valloc(proc->vctx, pages, vflags);
	} else {
		mapped = valloc(proc->vctx, pages, vflags);
	}

	if (!mapped)
		return -ENOMEM;

	memset(mapped, 0, pages * PAGE_SIZE);
	return (int64_t)(uintptr_t)mapped;
}

int64_t sys_lseek(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	int64_t offset = (int64_t)args->rsi;
	int whence = (int)args->rdx;

	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
		return -EINVAL;

	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -EINVAL;

	spinlock_acquire(&proc->fd_lock);
	struct fileio *f = proc_fd_lookup_locked(proc, fd);
	if (!f) {
		spinlock_release(&proc->fd_lock);
		return -EBADF;
	}
	fio_retain(f);
	spinlock_release(&proc->fd_lock);

	int64_t base = 0;
	if (whence == SEEK_CUR)
		base = (int64_t)f->offset;
	else if (whence == SEEK_END)
		base = (int64_t)f->size;

	int64_t new_off = base + offset;
	if (new_off < 0) {
		close(f);
		return -EINVAL;
	}

	f->offset = (size_t)new_off;
	close(f);
	return new_off;
}

int64_t sys_munmap(const syscall_args_t *args)
{
	void *addr = (void *)args->rdi;
	size_t length = (size_t)args->rsi;

	if (!addr || length == 0)
		return -EINVAL;

	if (((uintptr_t)addr & (PAGE_SIZE - 1)) != 0)
		return -EINVAL;

	struct pcb *proc = syscall_current_process();
	if (!proc || !proc->vctx)
		return -EINVAL;

	vregion_t *region = vget(proc->vctx, (uint64_t)(uintptr_t)addr);
	if (!region || region->start != (uint64_t)(uintptr_t)addr)
		return -EINVAL;

	if (length > (region->pages * PAGE_SIZE))
		return -EINVAL;

	vfree(proc->vctx, addr);
	return 0;
}

int64_t sys_clock_get(const syscall_args_t *args)
{
	int clock = (int)args->rdi;
	int64_t *secs = (int64_t *)args->rsi;
	int64_t *nanos = (int64_t *)args->rdx;

	if (!secs || !nanos)
		return -EFAULT;

	if (clock != CLOCK_REALTIME && clock != CLOCK_MONOTONIC)
		return -EINVAL;

	uint64_t ms = get_ms();
	*secs = (int64_t)(ms / 1000ull);
	*nanos = (int64_t)((ms % 1000ull) * 1000000ull);
	return 0;
}

int64_t sys_set_fs_base(const syscall_args_t *args)
{
	uintptr_t base = (uintptr_t)args->rdi;
	if (!base)
		return -EINVAL;
	trace("setfs_base(%.16llx)\n", base);

#if defined(__x86_64__)
	tcb *current = thread_current();
	if (!current)
		return -EINVAL;

	current->kthread.fs_base = (uint64_t)base;
	wrmsr(MSR_IA32_FS_BASE, (uint64_t)base);
	return 0;
#else
	(void)base;
	return -ENOSYS;
#endif
}

void syscall_builtin_init(void)
{
	register_syscall(SYS_EXIT, sys_exit, "exit");
	register_syscall(SYS_OPEN, sys_open, "open");
	register_syscall(SYS_READ, sys_read, "read");
	register_syscall(SYS_WRITE, sys_write, "write");
	register_syscall(SYS_CLOSE, sys_close, "close");
	register_syscall(SYS_MOUNT, sys_mount, "mount");
	register_syscall(SYS_IOCTL, sys_ioctl, "ioctl");
	register_syscall(SYS_LOAD_MODULE, sys_load_module, "load_module");
	register_syscall(SYS_EXEC, sys_exec, "exec");
	register_syscall(SYS_MMAP, sys_mmap, "mmap");
	register_syscall(SYS_LSEEK, sys_lseek, "lseek");
	register_syscall(SYS_MUNMAP, sys_munmap, "munmap");
	register_syscall(SYS_CLOCK_GET, sys_clock_get, "clock_get");
	register_syscall(SYS_SET_FS_BASE, sys_set_fs_base, "set_fs_base");
}
