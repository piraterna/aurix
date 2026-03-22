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
#include <mm/pmm.h>
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

#define FSFD_TARGET_NONE 0
#define FSFD_TARGET_PATH 1
#define FSFD_TARGET_FD 2
#define FSFD_TARGET_FD_PATH 3

#define AT_FDCWD -100

#define DIR_HANDLE_MAX_ENTRIES 256

#define EXEC_MAX_ARGS 128
#define EXEC_MAX_ENVP 128
#define EXEC_MAX_STRLEN 4096

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
		if (!vreserve(child->vctx, region->start, region->pages, vflags))
			return -ENOMEM;

		for (size_t i = 0; i < region->pages; i++) {
			uintptr_t virt = region->start + (i * PAGE_SIZE);
			uint64_t pflags = vget_flags(parent->pm, virt);
			if (!(pflags & VMM_PRESENT))
				continue;

			uintptr_t src_phys = vget_phys(parent->pm, virt);
			if (!src_phys)
				continue;
			uintptr_t phys_page = ALIGN_DOWN(src_phys, PAGE_SIZE);
			uint64_t new_flags = pflags;
			if (pflags & VMM_WRITABLE)
				new_flags = (pflags & ~VMM_WRITABLE) | VMM_COW;
			else if (pflags & VMM_COW)
				new_flags = pflags & ~VMM_WRITABLE;

			pmm_ref_inc(phys_page, 1);
			map_page(child->pm, virt, phys_page, new_flags);
			if (new_flags != pflags)
				map_page(parent->pm, virt, phys_page, new_flags);
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

static size_t bounded_strlen(const char *s, size_t max)
{
	if (!s)
		return 0;
	for (size_t i = 0; i < max; i++) {
		if (s[i] == '\0')
			return i;
	}
	return max;
}

static void free_string_vector(char **vec, size_t count)
{
	if (!vec)
		return;
	for (size_t i = 0; i < count; i++) {
		if (vec[i])
			kfree(vec[i]);
	}
	kfree(vec);
}

static int copy_user_string_vector(const char *const *user, size_t max,
								   char ***out_vec, size_t *out_count)
{
	if (!out_vec || !out_count)
		return -EINVAL;
	*out_vec = NULL;
	*out_count = 0;

	if (!user)
		return 0;

	char **vec = kmalloc(sizeof(char *) * max);
	if (!vec)
		return -ENOMEM;
	memset(vec, 0, sizeof(char *) * max);

	for (size_t i = 0; i < max; i++) {
		const char *src = user[i];
		if (!src) {
			*out_vec = vec;
			*out_count = i;
			return 0;
		}

		size_t len = bounded_strlen(src, EXEC_MAX_STRLEN);
		if (len == EXEC_MAX_STRLEN) {
			free_string_vector(vec, i);
			return -E2BIG;
		}

		char *dst = kmalloc(len + 1);
		if (!dst) {
			free_string_vector(vec, i);
			return -ENOMEM;
		}
		memcpy(dst, src, len);
		dst[len] = '\0';
		vec[i] = dst;
	}

	free_string_vector(vec, max);
	return -E2BIG;
}

static void dir_handle_free(struct fileio *file)
{
	if (!file || !file->dir)
		return;

	if (file->dir->entries)
		kfree(file->dir->entries);
	kfree(file->dir);
	file->dir = NULL;
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

int64_t sys_opendir(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	int *handle = (int *)args->rsi;

	if (!path || !handle)
		return -EFAULT;

	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -EINVAL;

	char *resolved = syscall_resolve_path(proc, path);
	if (!resolved)
		return -ENOMEM;

	struct fileio *f = open(resolved, O_RDONLY | O_DIRECTORY, 0);
	kfree(resolved);
	if (!f)
		return -ENOENT;

	dir_handle_t *dir = kmalloc(sizeof(dir_handle_t));
	if (!dir) {
		close(f);
		return -ENOMEM;
	}
	memset(dir, 0, sizeof(dir_handle_t));
	dir->entries = kmalloc(sizeof(struct dirent) * DIR_HANDLE_MAX_ENTRIES);
	if (!dir->entries) {
		kfree(dir);
		close(f);
		return -ENOMEM;
	}
	memset(dir->entries, 0, sizeof(struct dirent) * DIR_HANDLE_MAX_ENTRIES);
	dir->vnode = (struct vnode *)f->private;
	dir->count = 0;
	dir->index = 0;

	f->dir = dir;
	f->flags |= O_DIRECTORY;

	spinlock_acquire(&proc->fd_lock);
	int fd = proc_fd_alloc_locked(proc, f);
	spinlock_release(&proc->fd_lock);
	if (fd < 0) {
		dir_handle_free(f);
		close(f);
		return fd;
	}

	*handle = fd;
	return 0;
}

int64_t sys_read_entries(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	void *buffer = (void *)args->rsi;
	size_t max_size = (size_t)args->rdx;
	size_t *bytes_read = (size_t *)args->r10;

	if (!bytes_read)
		return -EFAULT;
	if (!buffer && max_size)
		return -EFAULT;

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

	if (!(f->flags & O_DIRECTORY) || !f->dir) {
		close(f);
		return -ENOTDIR;
	}

	dir_handle_t *dir = f->dir;
	if (!dir->entries) {
		close(f);
		return -ENOMEM;
	}

	if (dir->count == 0 && dir->index == 0) {
		size_t count = DIR_HANDLE_MAX_ENTRIES;
		int ret = vfs_readdir(dir->vnode, dir->entries, &count);
		if (ret != 0) {
			close(f);
			return -EINVAL;
		}
		dir->count = count;
	}

	size_t max_entries = max_size / sizeof(struct dirent);
	if (max_entries == 0 || dir->index >= dir->count) {
		*bytes_read = 0;
		close(f);
		return 0;
	}

	size_t remaining = dir->count - dir->index;
	size_t to_copy = remaining < max_entries ? remaining : max_entries;
	memcpy(buffer, &dir->entries[dir->index], to_copy * sizeof(struct dirent));
	dir->index += to_copy;
	*bytes_read = to_copy * sizeof(struct dirent);

	close(f);
	return 0;
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

	dir_handle_free(f);
	int r = close(f);
	if (r < 0) {
		error("close(): failed to close file descriptor %d (%s)\n", fd,
			  ERRNO_NAME(-r));
		return r;
	}

	return 0;
}

int64_t sys_stat(const syscall_args_t *args)
{
	int target = (int)args->rdi;
	int fd = (int)args->rsi;
	const char *path = (const char *)args->rdx;
	int flags = (int)args->r10;
	struct stat *st = (struct stat *)args->r8;

	(void)flags;

	if (!st)
		return -EFAULT;

	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -EINVAL;

	if (target == FSFD_TARGET_FD) {
		spinlock_acquire(&proc->fd_lock);
		struct fileio *f = proc_fd_lookup_locked(proc, fd);
		if (!f) {
			spinlock_release(&proc->fd_lock);
			return -EBADF;
		}
		fio_retain(f);
		spinlock_release(&proc->fd_lock);

		struct vnode *vnode = (struct vnode *)f->private;
		int ret = -EINVAL;
		if (vnode && vnode->ops && vnode->ops->getattr)
			ret = vnode->ops->getattr(vnode, st);

		close(f);
		return ret == 0 ? 0 : -EINVAL;
	}

	if (target == FSFD_TARGET_FD_PATH && fd != AT_FDCWD) {
		return -ENOSYS;
	}

	if (target != FSFD_TARGET_PATH && target != FSFD_TARGET_FD_PATH)
		return -EINVAL;

	if (!path)
		return -EFAULT;

	char *resolved = syscall_resolve_path(proc, path);
	if (!resolved)
		return -ENOMEM;

	int ret = vfs_stat(resolved, st);
	kfree(resolved);

	return ret == 0 ? 0 : -ENOENT;
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
		kfree(resolved);
		return -EINVAL;
	}

	char *buf = (char *)kmalloc(f->size);
	if (!buf) {
		close(f);
		kfree(resolved);
		return -ENOMEM;
	}

	if (read(f, f->size, buf) != f->size) {
		close(f);
		kfree(buf);
		kfree(resolved);
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
	if (!elf_load_user_process(buf, resolved, proc, NULL, 0, NULL, 0, &entry)) {
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

	thread_exit(thread_current(), code);
	__builtin_unreachable();
}

int64_t sys_execve(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	const char *const *argv = (const char *const *)args->rsi;
	const char *const *envp = (const char *const *)args->rdx;

	if (!path)
		return -EFAULT;

	tcb *current = thread_current();
	struct pcb *cur = current ? current->process : NULL;
	if (!cur)
		return -EINVAL;

	if (cur->threads && cur->threads->proc_next)
		return -EBUSY;

	char *resolved = syscall_resolve_path(cur, path);
	if (!resolved)
		return -ENOMEM;

	char **argv_copy = NULL;
	size_t argv_count = 0;
	int arg_ret =
		copy_user_string_vector(argv, EXEC_MAX_ARGS, &argv_copy, &argv_count);
	if (arg_ret != 0) {
		kfree(resolved);
		return arg_ret;
	}

	char **envp_copy = NULL;
	size_t envp_count = 0;
	int env_ret =
		copy_user_string_vector(envp, EXEC_MAX_ENVP, &envp_copy, &envp_count);
	if (env_ret != 0) {
		free_string_vector(argv_copy, argv_count);
		kfree(resolved);
		return env_ret;
	}

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

	pagetable *new_pm = create_pagemap();
	if (!new_pm) {
		free_string_vector(argv_copy, argv_count);
		free_string_vector(envp_copy, envp_count);
		kfree(resolved);
		kfree(buf);
		return -ENOMEM;
	}

	vctx_t *new_vctx = vinit(new_pm, 0x1000);
	if (!new_vctx) {
		destroy_pagemap(new_pm);
		free_string_vector(argv_copy, argv_count);
		free_string_vector(envp_copy, envp_count);
		kfree(resolved);
		kfree(buf);
		return -ENOMEM;
	}

	pagetable *old_pm = cur->pm;
	vctx_t *old_vctx = cur->vctx;
	char *old_image_elf = cur->image_elf;
	size_t old_image_size = cur->image_size;
	uintptr_t old_image_phys_base = cur->image_phys_base;
	uintptr_t old_image_load_base = cur->image_load_base;
	uintptr_t old_image_link_base = cur->image_link_base;
	size_t old_image_exec_size = cur->image_exec_size;
	uintptr_t old_user_stack_base = cur->user_stack_base;
	size_t old_user_stack_size = cur->user_stack_size;
	uintptr_t old_user_rsp = cur->user_rsp;

	cur->pm = new_pm;
	cur->vctx = new_vctx;
	cur->image_elf = NULL;
	cur->image_size = 0;
	cur->image_phys_base = 0;
	cur->image_load_base = 0;
	cur->image_link_base = 0;
	cur->image_exec_size = 0;
	cur->user_stack_base = 0;
	cur->user_stack_size = 0;
	cur->user_rsp = 0;

	uintptr_t entry = 0;
	if (!elf_load_user_process(
			buf, resolved, cur, (const char *const *)argv_copy, argv_count,
			(const char *const *)envp_copy, envp_count, &entry)) {
		cur->pm = old_pm;
		cur->vctx = old_vctx;
		cur->image_elf = old_image_elf;
		cur->image_size = old_image_size;
		cur->image_phys_base = old_image_phys_base;
		cur->image_load_base = old_image_load_base;
		cur->image_link_base = old_image_link_base;
		cur->image_exec_size = old_image_exec_size;
		cur->user_stack_base = old_user_stack_base;
		cur->user_stack_size = old_user_stack_size;
		cur->user_rsp = old_user_rsp;
		vdestroy(new_vctx);
		destroy_pagemap(new_pm);
		free_string_vector(argv_copy, argv_count);
		free_string_vector(envp_copy, envp_count);
		kfree(resolved);
		kfree(buf);
		return -EINVAL;
	}

	struct tcb *thread = thread_create_user(cur, (void (*)(void))entry);
	if (!thread) {
		cur->pm = old_pm;
		cur->vctx = old_vctx;
		cur->image_elf = old_image_elf;
		cur->image_size = old_image_size;
		cur->image_phys_base = old_image_phys_base;
		cur->image_load_base = old_image_load_base;
		cur->image_link_base = old_image_link_base;
		cur->image_exec_size = old_image_exec_size;
		cur->user_stack_base = old_user_stack_base;
		cur->user_stack_size = old_user_stack_size;
		cur->user_rsp = old_user_rsp;
		vdestroy(new_vctx);
		destroy_pagemap(new_pm);
		free_string_vector(argv_copy, argv_count);
		free_string_vector(envp_copy, envp_count);
		kfree(resolved);
		kfree(buf);
		return -ENOMEM;
	}

	char *name_copy = kmalloc(strlen(resolved) + 1);
	if (name_copy) {
		strcpy(name_copy, resolved);
		if (cur->name)
			kfree((void *)cur->name);
		cur->name = (const char *)name_copy;
	}

#if defined(__x86_64__)
	write_cr3((uint64_t)cur->pm);
	current->kthread.cr3 = (uint64_t)cur->pm;
#endif

	if (old_vctx)
		vdestroy(old_vctx);
	if (old_pm)
		destroy_pagemap(old_pm);

	kfree(resolved);
	kfree(buf);
	free_string_vector(argv_copy, argv_count);
	free_string_vector(envp_copy, envp_count);

	thread_exit(current, 0);
	__builtin_unreachable();
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
	if (!args)
		return -EINVAL;

	tcb *parent_thread = thread_current();
	if (!parent_thread || !parent_thread->process)
		return -EINVAL;

	pcb *parent = parent_thread->process;
	pcb *child = proc_create();
	if (!child)
		return -ENOMEM;

	child->parent_pid = parent->pid;
	child->user_stack_base = parent->user_stack_base;
	child->user_stack_size = parent->user_stack_size;
	child->user_rsp = args->rsp;
	child->image_elf = parent->image_elf;
	child->image_size = parent->image_size;
	child->image_phys_base = parent->image_phys_base;
	child->image_load_base = parent->image_load_base;
	child->image_link_base = parent->image_link_base;
	child->image_exec_size = parent->image_exec_size;

	if (child->cwd) {
		kfree(child->cwd);
		child->cwd = NULL;
	}
	if (parent->cwd)
		child->cwd = strdup(parent->cwd);

	if (child->name) {
		kfree((void *)child->name);
		child->name = NULL;
	}
	if (parent->name)
		child->name = strdup(parent->name);

	for (int fd = 0; fd < PROC_MAX_FDS; fd++) {
		if (child->fds[fd]) {
			close(child->fds[fd]);
			child->fds[fd] = NULL;
		}
	}

	spinlock_acquire(&parent->fd_lock);
	for (int fd = 0; fd < PROC_MAX_FDS; fd++) {
		struct fileio *f = parent->fds[fd];
		if (!f)
			continue;
		fio_retain(f);
		child->fds[fd] = f;
	}
	spinlock_release(&parent->fd_lock);

	int mem_err = syscall_clone_memory(parent, child);
	if (mem_err != 0) {
		proc_destroy(child);
		return mem_err;
	}

	tcb *child_thread = thread_clone_user(child, parent_thread);
	if (!child_thread) {
		proc_destroy(child);
		return -ENOMEM;
	}

	struct fork_frame {
		uint64_t rdi;
		uint64_t rsi;
		uint64_t rdx;
		uint64_t r10;
		uint64_t r8;
		uint64_t r9;
		uint64_t rcx;
		uint64_t r11;
		uint64_t rsp;
	};

	struct fork_frame frame = {
		.rdi = args->rdi,
		.rsi = args->rsi,
		.rdx = args->rdx,
		.r10 = args->r10,
		.r8 = args->r8,
		.r9 = args->r9,
		.rcx = args->rip,
		.r11 = args->rflags,
		.rsp = args->rsp,
	};

	uint64_t *rsp = (uint64_t *)(uintptr_t)child_thread->kthread.rsp0;

	*--rsp = frame.rsp;
	*--rsp = frame.r11;
	*--rsp = frame.rcx;
	*--rsp = frame.r9;
	*--rsp = frame.r8;
	*--rsp = frame.r10;
	*--rsp = frame.rdx;
	*--rsp = frame.rsi;
	*--rsp = frame.rdi;

	*--rsp = (uint64_t)(uintptr_t)fork_trampoline;
	*--rsp = args->rbx;
	*--rsp = args->rbp;
	*--rsp = args->r12;
	*--rsp = args->r13;
	*--rsp = args->r14;
	*--rsp = args->r15;
	*--rsp = 0x202;

	child_thread->kthread.rsp = (uint64_t)(uintptr_t)rsp;
	thread_enqueue(child_thread);

	return (int64_t)child->pid;
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
	register_syscall(SYS_OPENDIR, sys_opendir, "opendir");
	register_syscall(SYS_READENTRIES, sys_read_entries, "read_entries");
	register_syscall(SYS_STAT, sys_stat, "stat");
}
