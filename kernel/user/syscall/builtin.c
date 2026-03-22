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
#include <aurix.h>

#if defined(__x86_64__)
#include <arch/cpu/cpu.h>
#include <arch/cpu/syscall.h>
#endif

#include <loader/elf.h>

#define FD_RESERVED_STDIN 0
#define FD_STDOUT 1
#define FD_FIRST_DYNAMIC 2

#define PROT_NONE 0x0
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

static bool vregion_overlaps(vctx_t *ctx, uint64_t vaddr, size_t pages)
{
	if (!ctx || !ctx->root || pages == 0)
		return false;

	uint64_t size = pages * PAGE_SIZE;
	if (size / PAGE_SIZE != pages)
		return true;

	uint64_t vend = vaddr + size;
	if (vend < vaddr)
		return true;

	for (vregion_t *region = ctx->root; region; region = region->next) {
		if (region->pages == 0)
			continue;
		uint64_t rstart = region->start;
		uint64_t rend = region->start + (region->pages * PAGE_SIZE);
		if (vaddr < rend && vend > rstart)
			return true;
	}

	return false;
}

static char *syscall_normalize_path(const char *path)
{
	if (!path)
		return NULL;

	size_t len = strlen(path);
	bool abs = (len > 0 && path[0] == '/');

	char *temp = kmalloc(len + 1);
	if (!temp)
		return NULL;
	memcpy(temp, path, len + 1);

	size_t max_parts = (len / 2) + 2;
	char **parts = kmalloc(max_parts * sizeof(char *));
	if (!parts) {
		kfree(temp);
		return NULL;
	}

	size_t count = 0;
	char *save = NULL;
	char *token = strtok_r(temp, "/", &save);
	while (token) {
		if (strcmp(token, ".") == 0 || *token == '\0') {
			// skip
		} else if (strcmp(token, "..") == 0) {
			if (count > 0 && strcmp(parts[count - 1], "..") != 0) {
				count--;
			} else if (!abs) {
				parts[count++] = token;
			}
		} else {
			parts[count++] = token;
		}
		token = strtok_r(NULL, "/", &save);
	}

	size_t out_len = abs ? 1 : 0;
	for (size_t i = 0; i < count; i++)
		out_len += strlen(parts[i]) + 1;
	if (out_len == 0)
		out_len = 1;

	char *out = kmalloc(out_len + 1);
	if (!out) {
		kfree(parts);
		kfree(temp);
		return NULL;
	}

	char *cursor = out;
	if (abs)
		*cursor++ = '/';

	for (size_t i = 0; i < count; i++) {
		size_t part_len = strlen(parts[i]);
		memcpy(cursor, parts[i], part_len);
		cursor += part_len;
		if (i + 1 < count)
			*cursor++ = '/';
	}

	if (cursor == out) {
		*cursor++ = '/';
	}
	*cursor = '\0';

	kfree(parts);
	kfree(temp);
	return out;
}

static char *syscall_resolve_path(struct pcb *proc, const char *path)
{
	if (!path)
		return NULL;
	if (path[0] == '/')
		return syscall_normalize_path(path);

	const char *cwd = (proc && proc->cwd) ? proc->cwd : "/";
	size_t cwd_len = strlen(cwd);
	size_t path_len = strlen(path);
	bool need_slash = (cwd_len == 0 || cwd[cwd_len - 1] != '/');

	size_t total = cwd_len + (need_slash ? 1 : 0) + path_len + 1;
	char *combined = kmalloc(total);
	if (!combined)
		return NULL;
	memcpy(combined, cwd, cwd_len);
	size_t pos = cwd_len;
	if (need_slash)
		combined[pos++] = '/';
	memcpy(combined + pos, path, path_len);
	combined[pos + path_len] = '\0';

	char *normalized = syscall_normalize_path(combined);
	kfree(combined);
	return normalized;
}

static uint64_t pflags_to_vflags(uint64_t pflags)
{
	uint64_t vflags = VALLOC_READ;
	if (pflags & VMM_WRITABLE)
		vflags |= VALLOC_WRITE;
	if (pflags & VMM_USER)
		vflags |= VALLOC_USER;
	if (!(pflags & VMM_NX))
		vflags |= VALLOC_EXEC;
	if (!(pflags & VMM_PRESENT))
		vflags |= VALLOC_NO_PRESENT;
	return vflags;
}

static int syscall_clone_memory(struct pcb *parent, struct pcb *child)
{
	if (!parent || !child || !parent->vctx || !child->vctx)
		return -EINVAL;

	for (vregion_t *region = parent->vctx->root; region;
		 region = region->next) {
		if (region->pages == 0)
			continue;

		uint64_t vflags = pflags_to_vflags(region->flags);
		if (!(region->flags & VMM_PRESENT)) {
			if (!vreserve(child->vctx, region->start, region->pages, vflags))
				return -ENOMEM;
			continue;
		}

		uint64_t phys = (uint64_t)palloc(region->pages);
		if (!phys)
			return -ENOMEM;

		if (!vadd(child->vctx, region->start, phys, region->pages, vflags)) {
			pfree((void *)phys, region->pages);
			return -ENOMEM;
		}

		for (size_t i = 0; i < region->pages; i++) {
			uintptr_t virt = region->start + (i * PAGE_SIZE);
			uintptr_t src_phys = vget_phys(parent->pm, virt);
			void *dst = (void *)PHYS_TO_VIRT(phys + (i * PAGE_SIZE));
			if (src_phys) {
				void *src = (void *)PHYS_TO_VIRT(src_phys);
				memcpy(dst, src, PAGE_SIZE);
			} else {
				memset(dst, 0, PAGE_SIZE);
			}
		}
	}

	return 0;
}

static struct fileio *proc_fd_lookup_locked(struct pcb *proc, int fd)
{
	if (!proc || fd < 0 || fd >= PROC_MAX_FDS)
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

	char *resolved = syscall_resolve_path(proc, path);
	if (!resolved)
		return -ENOMEM;

	struct fileio *f = open(resolved, flags, mode);
	kfree(resolved);
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

	struct pcb *proc = syscall_current_process();
	char *resolved = syscall_resolve_path(proc, path);
	if (!resolved)
		return -ENOMEM;

	struct fileio *f = open(resolved, O_RDONLY, 0);
	kfree(resolved);
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

	struct pcb *cur = syscall_current_process();
	char *resolved = syscall_resolve_path(cur, path);
	if (!resolved)
		return -ENOMEM;

	struct fileio *f = open(resolved, O_RDONLY, 0);
	if (!f) {
		kfree(resolved);
		return -ENOENT;
	}

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

	char *name_copy = kmalloc(strlen(resolved) + 1);
	if (name_copy)
		strcpy(name_copy, resolved);
	proc->name = (const char *)name_copy;

	uintptr_t entry = 0;
	if (!elf_load_user_process(buf, resolved, proc, &entry)) {
		kfree(resolved);
		kfree(buf);
		proc_destroy(proc);
		return -EINVAL;
	}

	kfree(resolved);

	kfree(buf);

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

int64_t sys_execve(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	const char *const *argv = (const char *const *)args->rsi;
	const char *const *envp = (const char *const *)args->rdx;

	(void)argv;
	(void)envp;

	if (!path)
		return -EFAULT;

	struct pcb *cur = syscall_current_process();
	char *resolved = syscall_resolve_path(cur, path);
	if (!resolved)
		return -ENOMEM;

	struct fileio *f = open(resolved, O_RDONLY, 0);
	if (!f) {
		kfree(resolved);
		return -ENOENT;
	}

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

	char *name_copy = kmalloc(strlen(resolved) + 1);
	if (name_copy)
		strcpy(name_copy, resolved);
	proc->name = (const char *)name_copy;

	uintptr_t entry = 0;
	if (!elf_load_user_process(buf, resolved, proc, &entry)) {
		kfree(resolved);
		kfree(buf);
		proc_destroy(proc);
		return -EINVAL;
	}

	kfree(resolved);
	kfree(buf);

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
	if (prot == PROT_NONE)
		vflags |= VALLOC_NO_PRESENT;
	if (prot & PROT_READ)
		vflags |= VALLOC_READ;
	if (prot & PROT_WRITE)
		vflags |= VALLOC_WRITE;
	if (prot & PROT_EXEC)
		vflags |= VALLOC_EXEC;

	void *mapped = NULL;
	uintptr_t min_addr = VPM_MIN_ADDR;
	uintptr_t hint = (uintptr_t)addr;

	if ((flags & MAP_FIXED) && (flags & MAP_FIXED_NOREPLACE))
		return -EINVAL;

	if (flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) {
		if (!addr)
			return -EINVAL;
		if (!IS_PAGE_ALIGNED(addr))
			return -EINVAL;
		if (hint < min_addr)
			return -EINVAL;
		if (flags & MAP_FIXED_NOREPLACE) {
			if (vregion_overlaps(proc->vctx, hint, pages))
				return -EEXIST;
		} else {
			vfree_range(proc->vctx, hint, pages);
		}
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

	size_t pages = DIV_ROUND_UP(length, PAGE_SIZE);
	if (pages == 0)
		return -EINVAL;

	vfree_range(proc->vctx, (uint64_t)(uintptr_t)addr, pages);
	return 0;
}

int64_t sys_mprotect(const syscall_args_t *args)
{
	void *addr = (void *)args->rdi;
	size_t length = (size_t)args->rsi;
	int prot = (int)args->rdx;

	if (!addr || length == 0)
		return -EINVAL;

	if (!IS_PAGE_ALIGNED(addr))
		return -EINVAL;

	if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
		return -EINVAL;

	struct pcb *proc = syscall_current_process();
	if (!proc || !proc->vctx || !proc->pm)
		return -EINVAL;

	size_t pages = DIV_ROUND_UP(length, PAGE_SIZE);
	if (pages == 0)
		return -EINVAL;

	uint64_t vflags = VALLOC_USER;
	if (prot == PROT_NONE) {
		vflags |= VALLOC_NO_PRESENT;
	} else {
		if (prot & PROT_READ)
			vflags |= VALLOC_READ;
		if (prot & PROT_WRITE)
			vflags |= VALLOC_WRITE;
		if (prot & PROT_EXEC)
			vflags |= VALLOC_EXEC;
	}

	uint64_t pflags = VFLAGS_TO_PFLAGS(vflags);
	uintptr_t base = (uintptr_t)addr;

	for (size_t i = 0; i < pages; i++) {
		uintptr_t virt = base + (i * PAGE_SIZE);
		uintptr_t phys = vget_phys(proc->pm, virt);
		if (!phys)
			return -ENOMEM;
		map_page(proc->pm, virt, ALIGN_DOWN(phys, PAGE_SIZE), pflags);
	}

	for (vregion_t *region = proc->vctx->root; region; region = region->next) {
		if (region->pages == 0)
			continue;
		uintptr_t rstart = region->start;
		uintptr_t rend = region->start + (region->pages * PAGE_SIZE);
		if (base <= rstart && (base + (pages * PAGE_SIZE)) >= rend)
			region->flags = pflags;
	}

	return 0;
}

int64_t sys_getcwd(const syscall_args_t *args)
{
	char *buf = (char *)args->rdi;
	size_t size = (size_t)args->rsi;

	if (!buf || size == 0)
		return -EINVAL;

	struct pcb *proc = syscall_current_process();
	const char *cwd = (proc && proc->cwd) ? proc->cwd : "/";
	size_t len = strlen(cwd);
	if (len + 1 > size)
		return -ERANGE;
	memcpy(buf, cwd, len + 1);
	return 0;
}

int64_t sys_fork(const syscall_args_t *args)
{
	(void)args;
	kprintf("fork() called\n");
	return -ENOSYS;
}

int64_t sys_chdir(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	if (!path)
		return -EFAULT;

	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -EINVAL;

	char *resolved = syscall_resolve_path(proc, path);
	if (!resolved)
		return -ENOMEM;

	struct vnode *vnode = NULL;
	int ret = vfs_lookup(resolved, &vnode);
	if (ret != 0) {
		kfree(resolved);
		return -ENOENT;
	}

	if (!vnode || vnode->vtype != VNODE_DIR) {
		vnode_unref(vnode);
		kfree(resolved);
		return -ENOTDIR;
	}

	vnode_unref(vnode);
	if (proc->cwd)
		kfree(proc->cwd);
	proc->cwd = resolved;
	return 0;
}

int64_t sys_waitpid(const syscall_args_t *args)
{
	int pid = (int)args->rdi;
	int *status = (int *)args->rsi;
	int options = (int)args->rdx;

	if (pid <= 0)
		return -EINVAL;
	if (options != 0)
		return -EINVAL;

	struct pcb *parent = syscall_current_process();
	if (!parent)
		return -EINVAL;

	struct pcb *child = proc_get_by_pid((uint32_t)pid);
	if (!child)
		return -ECHILD;
	if (child->parent_pid != parent->pid)
		return -ECHILD;

	while (!child->exited)
		sched_yield();

	if (status)
		*status = (child->exit_code & 0xff) << 8;

	proc_destroy(child);
	return pid;
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
	register_syscall(SYS_MPROTECT, sys_mprotect, "mprotect");
	register_syscall(SYS_CLOCK_GET, sys_clock_get, "clock_get");
	register_syscall(SYS_SET_FS_BASE, sys_set_fs_base, "set_fs_base");
	register_syscall(SYS_GETCWD, sys_getcwd, "getcwd");
	register_syscall(SYS_FORK, sys_fork, "fork");
	register_syscall(SYS_CHDIR, sys_chdir, "chdir");
	register_syscall(SYS_WAITPID, sys_waitpid, "waitpid");
	register_syscall(SYS_EXECVE, sys_execve, "execve");
}
