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

#define SYS_EXIT 0
#define SYS_OPEN 1
#define SYS_READ 2
#define SYS_WRITE 3
#define SYS_CLOSE 4
#define SYS_MOUNT 5
#define SYS_IOCTL 6
#define SYS_LOAD_MODULE 7

int64_t sys_exit(void *args)
{
	syscall_args_t *sys_args = (syscall_args_t *)args;
	int64_t code = (int64_t)sys_args->rdi;
	info("TID=%u (%s) exited with code %lld\n", thread_current()->tid,
		 thread_current()->process->name ? thread_current()->process->name :
										   "unknown",
		 code);
	thread_exit(thread_current(), (int)code);
	return 0;
}

int64_t sys_open(void *args)
{
	syscall_args_t *sys_args = (syscall_args_t *)args;
	const char *path = (const char *)sys_args->rdi;
	int flags = (int)sys_args->rsi;
	mode_t mode = (mode_t)sys_args->rdx;

	if (!path) {
		error("open(): invalid path (%s)\n", ERRNO_NAME(EFAULT));
		return -EFAULT;
	}

	struct fileio *f = open(path, flags, mode);
	if (!f) {
		error("open(): failed to open %s (%s)\n", path, ERRNO_NAME(ENOENT));
		return -ENOENT;
	}

	trace("open(): opened %s with flags %d and mode %o\n", path, flags, mode);
	return (int64_t)f;
}

int64_t sys_read(void *args)
{
	syscall_args_t *sys_args = (syscall_args_t *)args;
	struct fileio *f = (struct fileio *)sys_args->rdi;
	if (!f) {
		error("read(): invalid file descriptor (%s)\n", ERRNO_NAME(EBADF));
		return -EBADF;
	}

	void *buf = (void *)sys_args->rsi;
	size_t count = (size_t)sys_args->rdx;
	if (!buf && count) {
		error("read(): invalid buffer (%s)\n", ERRNO_NAME(EFAULT));
		return -EFAULT;
	}

	return (int64_t)read(f, count, buf);
}

int64_t sys_write(void *args)
{
	syscall_args_t *sys_args = (syscall_args_t *)args;
	struct fileio *f = (struct fileio *)sys_args->rdi;
	if (!f) {
		error("write(): invalid file descriptor (%s)\n", ERRNO_NAME(EBADF));
		return -EBADF;
	}

	const void *buf = (const void *)sys_args->rsi;
	size_t count = (size_t)sys_args->rdx;
	if (!buf && count) {
		error("write(): invalid buffer (%s)\n", ERRNO_NAME(EFAULT));
		return -EFAULT;
	}

	int r = write(f, (void *)buf, count);

	if (r < 0) {
		error("write(): failed to write to file descriptor %p (%s)\n", f,
			  ERRNO_NAME(-r));
		return r;
	}
	return r;
}

int64_t sys_close(void *args)
{
	syscall_args_t *sys_args = (syscall_args_t *)args;
	struct fileio *f = (struct fileio *)sys_args->rdi;
	if (!f) {
		error("close(): invalid file descriptor (%s)\n", ERRNO_NAME(EBADF));
		return -EBADF;
	}

	int r = close(f);
	if (r < 0) {
		error("close(): failed to close file descriptor %p (%s)\n", f,
			  ERRNO_NAME(-r));
		return r;
	}

	return 0;
}

int64_t sys_mount(void *args)
{
	syscall_args_t *sys_args = (syscall_args_t *)args;
	const char *source = (const char *)sys_args->rdi;
	const char *target = (const char *)sys_args->rsi;
	const char *fstype = (const char *)sys_args->rdx;
	uint64_t flags = sys_args->r10;
	void *data = (void *)sys_args->r8;

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

int64_t sys_ioctl(void *args)
{
	syscall_args_t *sys_args = (syscall_args_t *)args;
	struct fileio *f = (struct fileio *)sys_args->rdi;
	int request = (int)sys_args->rsi;
	void *arg = (void *)sys_args->rdx;

	if (!f || !f->private) {
		return -EBADF;
	}

	int ret = vfs_ioctl((struct vnode *)f->private, request, arg);
	if (ret == -1) {
		return -ENOTTY;
	}

	return ret;
}

int64_t sys_load_module(void *args)
{
	syscall_args_t *sys_args = (syscall_args_t *)args;
	const char *path = (const char *)sys_args->rdi;

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
}
