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
#include <lib/string.h>
#include <sys/errno.h>
#include <loader/module.h>

#include <loader/elf.h>

#define FD_RESERVED_STDIN 0
#define FD_STDOUT 1
#define FD_FIRST_DYNAMIC 2

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

int64_t sys_sout(const syscall_args_t *args)
{
	const char *str = (const char *)args->rdi;
	if (!str) {
		return -EFAULT;
	}

	size_t len = (size_t)args->rsi;
	if (len == 0) {
		return 0;
	}

	kprintf("[sout()] %.*s", (int)len, str);
	return 0;
}

void syscall_builtin_init(void)
{
	register_syscall(SYS_EXIT, sys_exit);
	register_syscall(SYS_OPEN, sys_open);
	register_syscall(SYS_READ, sys_read);
	register_syscall(SYS_WRITE, sys_write);
	register_syscall(SYS_CLOSE, sys_close);
	register_syscall(SYS_MOUNT, sys_mount);
	register_syscall(SYS_IOCTL, sys_ioctl);
	register_syscall(SYS_LOAD_MODULE, sys_load_module);
	register_syscall(SYS_EXEC, sys_exec);
	register_syscall(SYS_SOUT, sys_sout);
}
