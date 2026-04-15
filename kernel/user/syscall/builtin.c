/*********************************************************************************/
/* Module Name:  builtin.c                                                       */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE.                                                                     */
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
#include <sys/types.h>
#include <loader/module.h>
#include <time/time.h>
#include <ipc/pipe.h>
#include <aurix.h>
#include <user/access.h>
#include <arch/cpu/switch.h>
#include <loader/elf.h>

extern void switch_enter_user(void);

#if defined(__x86_64__)
#include <arch/cpu/cpu.h>
#include <arch/cpu/syscall.h>
#include <arch/cpu/gdt.h>
#endif

#ifndef AT_FDCWD
#define AT_FDCWD -100
#endif
#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR 0x200
#endif
#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100
#endif
#ifndef AT_EACCESS
#define AT_EACCESS 0x200
#endif
#ifndef AT_EMPTY_PATH
#define AT_EMPTY_PATH 0x1000
#endif

#ifndef UTIME_NOW
#define UTIME_NOW 0x3fffffff
#endif
#ifndef UTIME_OMIT
#define UTIME_OMIT 0x3ffffffe
#endif

#define FD_RESERVED_STDIN 0
#define FD_STDOUT 1
#define FD_FIRST_DYNAMIC 3

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

#define FCNTL_F_DUPFD 0
#define FCNTL_F_GETFD 1
#define FCNTL_F_SETFD 2
#define FCNTL_F_GETFL 3
#define FCNTL_F_SETFL 4
#define FCNTL_F_DUPFD_CLOEXEC 1030

#define FD_CLOEXEC 1

#define POLLIN 0x0001
#define POLLOUT 0x0004
#define POLLERR 0x0008
#define POLLHUP 0x0010
#define POLLNVAL 0x0020

#define DIR_HANDLE_MAX_ENTRIES 256

#define EXEC_MAX_ARGS 128
#define EXEC_MAX_ENVP 128
#define EXEC_MAX_STRLEN 4096

#define SYSCALL_MAX_PATH 4096
#define SYSCALL_MAX_POLLFDS 1024

#if defined(__x86_64__)
#define MSR_IA32_FS_BASE 0xC0000100
#endif

#define SYSCALL_REQUIRE(cond, err) \
	do {                           \
		if (!(cond))               \
			return (err);          \
	} while (0)

#define SYSCALL_REQUIRE_PROC(proc) \
	do {                           \
		if (!(proc))               \
			return -EINVAL;        \
	} while (0)

#define SYSCALL_REQUIRE_FD(fd)                \
	do {                                      \
		if ((fd) < 0 || (fd) >= PROC_MAX_FDS) \
			return -EBADF;                    \
	} while (0)

#define SYSCALL_KFREE_IF(ptr) \
	do {                      \
		if ((ptr))            \
			kfree((ptr));     \
	} while (0)

#define SYSCALL_CLOSE_IF(f) \
	do {                    \
		if ((f))            \
			close((f));     \
	} while (0)

#define SYSCALL_UNLOCK_FD_IF(proc, held)        \
	do {                                        \
		if (held) {                             \
			spinlock_release(&(proc)->fd_lock); \
			held = false;                       \
		}                                       \
	} while (0)

struct pollfd {
	int fd;
	short events;
	short revents;
};

struct aurix_timespec {
	int64_t tv_sec;
	long tv_nsec;
};

static struct pcb *syscall_current_process(void)
{
	struct tcb *current = thread_current();
	if (!current)
		return NULL;
	return current->process;
}

static int syscall_user_readable(const void *ptr, size_t len)
{
	if (!ptr && len)
		return -EFAULT;
	return 0;
}

static int syscall_user_writable(void *ptr, size_t len)
{
	if (!ptr && len)
		return -EFAULT;
	return 0;
}

static int syscall_copy_from_user(void *dst, const void *src, size_t len)
{
	int r = syscall_user_readable(src, len);
	if (r != 0)
		return r;
	if (len)
		memcpy(dst, src, len);
	return 0;
}

static int syscall_copy_to_user(void *dst, const void *src, size_t len)
{
	int r = syscall_user_writable(dst, len);
	if (r != 0)
		return r;
	if (len)
		memcpy(dst, src, len);
	return 0;
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

static int syscall_copy_string_from_user(const char *user, char **out,
										 size_t max_len)
{
	SYSCALL_REQUIRE(out != NULL, -EINVAL);
	*out = NULL;

	SYSCALL_REQUIRE(user != NULL, -EFAULT);
	SYSCALL_REQUIRE(syscall_user_readable(user, 1) == 0, -EFAULT);

	size_t len = bounded_strlen(user, max_len);
	if (len == max_len)
		return -ENAMETOOLONG;

	char *k = kmalloc(len + 1);
	if (!k)
		return -ENOMEM;

	memcpy(k, user, len);
	k[len] = '\0';
	*out = k;
	return 0;
}

static bool syscall_is_privileged(const struct pcb *proc)
{
	return proc && proc->euid == 0;
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
			/* skip */
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

	if (cursor == out)
		*cursor++ = '/';
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

static int syscall_resolve_user_path(struct pcb *proc, const char *user_path,
									 char **resolved)
{
	char *kpath = NULL;
	int r = syscall_copy_string_from_user(user_path, &kpath, SYSCALL_MAX_PATH);
	if (r != 0)
		return r;

	char *full = syscall_resolve_path(proc, kpath);
	kfree(kpath);

	if (!full)
		return -ENOMEM;

	*resolved = full;
	return 0;
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
			if (new_flags != pflags) {
				map_page(parent->pm, virt, phys_page, new_flags);
#if defined(__x86_64__)
				__asm__ volatile("invlpg (%0)" ::"r"((void *)virt) : "memory");
#endif
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

static int proc_fd_alloc_from_locked(struct pcb *proc, struct fileio *file,
									 int start_fd)
{
	if (!proc || !file)
		return -EINVAL;

	if (start_fd < FD_FIRST_DYNAMIC)
		start_fd = FD_FIRST_DYNAMIC;

	for (int fd = start_fd; fd < PROC_MAX_FDS; fd++) {
		if (!proc->fds[fd]) {
			proc->fds[fd] = file;
			return fd;
		}
	}

	return -EMFILE;
}

static int proc_fd_alloc_locked(struct pcb *proc, struct fileio *file)
{
	return proc_fd_alloc_from_locked(proc, file, FD_FIRST_DYNAMIC);
}

static int syscall_fd_get(struct pcb *proc, int fd, struct fileio **out)
{
	SYSCALL_REQUIRE(out != NULL, -EINVAL);
	*out = NULL;

	SYSCALL_REQUIRE_PROC(proc);
	SYSCALL_REQUIRE_FD(fd);

	spinlock_acquire(&proc->fd_lock);
	struct fileio *f = proc_fd_lookup_locked(proc, fd);
	if (!f) {
		spinlock_release(&proc->fd_lock);
		return -EBADF;
	}
	fio_retain(f);
	spinlock_release(&proc->fd_lock);

	*out = f;
	return 0;
}

static int syscall_resolve_path_at(struct pcb *proc, int dirfd,
								   const char *path, char **resolved)
{
	if (!proc || !path || !resolved)
		return -EFAULT;

	*resolved = NULL;

	if (path[0] == '/' || dirfd == AT_FDCWD) {
		char *full = syscall_resolve_path(proc, path);
		if (!full)
			return -ENOMEM;
		*resolved = full;
		return 0;
	}

	spinlock_acquire(&proc->fd_lock);
	struct fileio *base_file = proc_fd_lookup_locked(proc, dirfd);
	if (!base_file || !base_file->private) {
		spinlock_release(&proc->fd_lock);
		return -EBADF;
	}
	fio_retain(base_file);
	spinlock_release(&proc->fd_lock);

	struct vnode *base_vnode = (struct vnode *)base_file->private;
	if (!base_vnode || base_vnode->vtype != VNODE_DIR || !base_vnode->path) {
		close(base_file);
		return -ENOTDIR;
	}

	const char *base = base_vnode->path;
	size_t base_len = strlen(base);
	size_t path_len = strlen(path);
	bool need_slash = (base_len == 0 || base[base_len - 1] != '/');

	size_t total = base_len + (need_slash ? 1 : 0) + path_len + 1;
	char *combined = kmalloc(total);
	if (!combined) {
		close(base_file);
		return -ENOMEM;
	}

	memcpy(combined, base, base_len);
	size_t pos = base_len;
	if (need_slash)
		combined[pos++] = '/';
	memcpy(combined + pos, path, path_len);
	combined[pos + path_len] = '\0';

	char *full = syscall_normalize_path(combined);
	kfree(combined);
	close(base_file);

	if (!full)
		return -ENOMEM;

	*resolved = full;
	return 0;
}

static int syscall_resolve_user_path_at(struct pcb *proc, int dirfd,
										const char *user_path, char **resolved)
{
	char *kpath = NULL;
	int r = syscall_copy_string_from_user(user_path, &kpath, SYSCALL_MAX_PATH);
	if (r != 0)
		return r;

	r = syscall_resolve_path_at(proc, dirfd, kpath, resolved);
	kfree(kpath);
	return r;
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

static char *dup_cstring(const char *src)
{
	if (!src)
		return NULL;
	size_t len = strlen(src);
	char *dst = kmalloc(len + 1);
	if (!dst)
		return NULL;
	memcpy(dst, src, len);
	dst[len] = '\0';
	return dst;
}

static int parse_shebang(const char *buf, size_t size, char **interp_out,
						 char **arg_out)
{
	if (interp_out)
		*interp_out = NULL;
	if (arg_out)
		*arg_out = NULL;
	if (!buf || size < 2)
		return 0;
	if (buf[0] != '#' || buf[1] != '!')
		return 0;

	size_t i = 2;
	while (i < size && (buf[i] == ' ' || buf[i] == '\t'))
		i++;
	if (i >= size || buf[i] == '\n' || buf[i] == '\r')
		return 0;

	size_t path_start = i;
	while (i < size && buf[i] != '\n' && buf[i] != '\r' && buf[i] != ' ' &&
		   buf[i] != '\t')
		i++;
	if (i == path_start)
		return 0;

	size_t path_len = i - path_start;
	char *interp = kmalloc(path_len + 1);
	if (!interp)
		return -ENOMEM;
	memcpy(interp, buf + path_start, path_len);
	interp[path_len] = '\0';

	while (i < size && (buf[i] == ' ' || buf[i] == '\t'))
		i++;

	char *arg = NULL;
	if (i < size && buf[i] != '\n' && buf[i] != '\r') {
		size_t arg_start = i;
		while (i < size && buf[i] != '\n' && buf[i] != '\r')
			i++;
		size_t arg_len = i - arg_start;
		arg = kmalloc(arg_len + 1);
		if (!arg) {
			kfree(interp);
			return -ENOMEM;
		}
		memcpy(arg, buf + arg_start, arg_len);
		arg[arg_len] = '\0';
	}

	if (interp_out)
		*interp_out = interp;
	else
		kfree(interp);

	if (arg_out)
		*arg_out = arg;
	else if (arg)
		kfree(arg);

	return 1;
}

static int build_shebang_argv(const char *interp, const char *interp_arg,
							  const char *script, char **orig,
							  size_t orig_count, char ***out_argv,
							  size_t *out_count)
{
	if (!interp || !script || !out_argv || !out_count)
		return -EINVAL;

	size_t tail = orig_count > 1 ? (orig_count - 1) : 0;
	size_t total = 1 + (interp_arg ? 1 : 0) + 1 + tail;
	if (total > EXEC_MAX_ARGS)
		return -E2BIG;

	char **vec = kmalloc(sizeof(char *) * total);
	if (!vec)
		return -ENOMEM;
	memset(vec, 0, sizeof(char *) * total);

	size_t idx = 0;
	vec[idx++] = dup_cstring(interp);
	if (!vec[idx - 1])
		goto fail;
	if (interp_arg) {
		vec[idx++] = dup_cstring(interp_arg);
		if (!vec[idx - 1])
			goto fail;
	}
	vec[idx++] = dup_cstring(script);
	if (!vec[idx - 1])
		goto fail;

	for (size_t i = 1; i < orig_count; i++) {
		vec[idx++] = dup_cstring(orig[i]);
		if (!vec[idx - 1])
			goto fail;
	}

	*out_argv = vec;
	*out_count = total;
	return 0;

fail:
	free_string_vector(vec, idx);
	return -ENOMEM;
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

	SYSCALL_REQUIRE(syscall_user_readable(user, sizeof(void *)) == 0, -EFAULT);

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

static int copy_file_data(const char *src, const char *dst, mode_t mode)
{
	struct fileio *in = open(src, O_RDONLY, 0);
	if (!in)
		return -ENOENT;

	struct fileio *out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (!out) {
		close(in);
		return -ENOENT;
	}

	char buf[4096];
	int status = 0;
	for (;;) {
		size_t n = read(in, sizeof(buf), buf);
		if (n == 0)
			break;
		int wrote = write(out, buf, n);
		if (wrote < 0 || (size_t)wrote != n) {
			status = -EIO;
			break;
		}
	}

	close(in);
	close(out);
	return status;
}

static bool mode_is_dir(mode_t mode)
{
	return (mode & S_IFMT) == S_IFDIR;
}

static bool mode_is_link(mode_t mode)
{
	return (mode & S_IFMT) == S_IFLNK;
}

static bool mode_is_reg(mode_t mode)
{
	return (mode & S_IFMT) == S_IFREG;
}

static uint32_t clamp_ts_sec(int64_t sec)
{
	if (sec < 0)
		return 0;
	if ((uint64_t)sec > 0xffffffffull)
		return 0xffffffffu;
	return (uint32_t)sec;
}

static void apply_utimens(struct stat *st, const struct aurix_timespec times[2])
{
	uint32_t now = (uint32_t)(get_ms() / 1000ull);

	if (!times) {
		st->st_atim = now;
		st->st_mtim = now;
		return;
	}

	if (times[0].tv_nsec != UTIME_OMIT)
		st->st_atim = (times[0].tv_nsec == UTIME_NOW) ?
						  now :
						  clamp_ts_sec(times[0].tv_sec);

	if (times[1].tv_nsec != UTIME_OMIT)
		st->st_mtim = (times[1].tv_nsec == UTIME_NOW) ?
						  now :
						  clamp_ts_sec(times[1].tv_sec);
}

static void exec_enter_current_user(tcb *current, pcb *proc, uintptr_t entry)
{
	if (!current || !proc)
		for (;;)
			cpu_halt();

	uint64_t *rsp = (uint64_t *)(uintptr_t)current->kthread.rsp0;
	rsp = (uint64_t *)((uintptr_t)rsp - 8);

	*--rsp = 0x1b;
	*--rsp = proc->user_rsp;
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

	current->kthread.rsp = (uint64_t)rsp;
	current->kthread.cr3 = (uint64_t)proc->pm;
	current->user = true;

#if defined(__x86_64__)
	gdt_set_kernel_stack(current->kthread.rsp0);
	write_cr3((uint64_t)proc->pm);
#endif

	struct kthread old_ctx;
	memset(&old_ctx, 0, sizeof(old_ctx));
	switch_task(&old_ctx, &current->kthread);
	__builtin_unreachable();
}

/*********************************************************************************/
/* Builtin system calls                                                           */
/*********************************************************************************/

int64_t sys_exit(const syscall_args_t *args)
{
	int64_t code = (int64_t)args->rdi;

	tcb *thr = thread_current();
	SYSCALL_REQUIRE(thr != NULL, -EINVAL);

	info("TID=%u (%s, userspace=%s) exited with code %lld\n", thr->tid,
		 (thr->process && thr->process->name) ? thr->process->name : "unknown",
		 thr->user ? "yes" : "no", code);

	thread_exit(thr, (int)code);
	return 0;
}

int64_t sys_open(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	int flags = (int)args->rsi;
	mode_t mode = (mode_t)args->rdx;

	SYSCALL_REQUIRE(path != NULL, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved = NULL;
	int r = syscall_resolve_user_path(proc, path, &resolved);
	if (r != 0)
		return r;

	struct fileio *f = open(resolved, flags, mode);
	kfree(resolved);
	if (!f)
		return -ENOENT;

	spinlock_acquire(&proc->fd_lock);
	int fd = proc_fd_alloc_locked(proc, f);
	spinlock_release(&proc->fd_lock);

	if (fd < 0) {
		close(f);
		return fd;
	}

	trace("open(): opened fd=%d flags=%d mode=%o\n", fd, flags, mode);
	return fd;
}

int64_t sys_opendir(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	int *handle = (int *)args->rsi;

	SYSCALL_REQUIRE(path != NULL, -EFAULT);
	SYSCALL_REQUIRE(handle != NULL, -EFAULT);
	SYSCALL_REQUIRE(syscall_user_writable(handle, sizeof(*handle)) == 0,
					-EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved = NULL;
	int r = syscall_resolve_user_path(proc, path, &resolved);
	if (r != 0)
		return r;

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

	return syscall_copy_to_user(handle, &fd, sizeof(fd));
}

int64_t sys_read_entries(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	void *buffer = (void *)args->rsi;
	size_t max_size = (size_t)args->rdx;
	size_t *bytes_read = (size_t *)args->r10;

	SYSCALL_REQUIRE(bytes_read != NULL, -EFAULT);
	SYSCALL_REQUIRE(!(buffer == NULL && max_size != 0), -EFAULT);
	SYSCALL_REQUIRE(syscall_user_writable(bytes_read, sizeof(*bytes_read)) == 0,
					-EFAULT);
	if (max_size)
		SYSCALL_REQUIRE(syscall_user_writable(buffer, max_size) == 0, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	struct fileio *f = NULL;
	int r = syscall_fd_get(proc, fd, &f);
	if (r != 0)
		return r;

	if (!f->dir) {
		struct vnode *vnode = (struct vnode *)f->private;
		if (!vnode || vnode->vtype != VNODE_DIR) {
			close(f);
			return -ENOTDIR;
		}

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
		dir->vnode = vnode;
		f->dir = dir;
		f->flags |= O_DIRECTORY;
	}

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
			return ret;
		}
		dir->count = count;
	}

	size_t max_entries = max_size / sizeof(struct dirent);
	if (max_entries == 0 || dir->index >= dir->count) {
		size_t zero = 0;
		close(f);
		return syscall_copy_to_user(bytes_read, &zero, sizeof(zero));
	}

	size_t remaining = dir->count - dir->index;
	size_t to_copy = remaining < max_entries ? remaining : max_entries;
	size_t copied_bytes = to_copy * sizeof(struct dirent);

	int ret =
		syscall_copy_to_user(buffer, &dir->entries[dir->index], copied_bytes);
	if (ret != 0) {
		close(f);
		return ret;
	}

	dir->index += to_copy;
	close(f);

	return syscall_copy_to_user(bytes_read, &copied_bytes,
								sizeof(copied_bytes));
}

int64_t sys_read(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	void *buf = (void *)args->rsi;
	size_t count = (size_t)args->rdx;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);
	SYSCALL_REQUIRE(!(buf == NULL && count != 0), -EFAULT);
	SYSCALL_REQUIRE(syscall_user_writable(buf, count) == 0, -EFAULT);

	struct fileio *f = NULL;
	int r = syscall_fd_get(proc, fd, &f);
	if (r != 0)
		return r;

	int64_t bytes = (int64_t)read(f, count, buf);
	close(f);
	return bytes;
}

int64_t sys_write(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	const void *buf = (const void *)args->rsi;
	size_t count = (size_t)args->rdx;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);
	SYSCALL_REQUIRE(!(buf == NULL && count != 0), -EFAULT);
	SYSCALL_REQUIRE(syscall_user_readable(buf, count) == 0, -EFAULT);

	struct fileio *f = NULL;
	int r = syscall_fd_get(proc, fd, &f);
	if (r != 0)
		return r;

	int wr = write(f, (void *)buf, count);
	close(f);
	return wr;
}

int64_t sys_close(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	SYSCALL_REQUIRE_FD(fd);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	spinlock_acquire(&proc->fd_lock);
	struct fileio *f = proc->fds[fd];
	if (!f) {
		spinlock_release(&proc->fd_lock);
		return -EBADF;
	}
	proc->fds[fd] = NULL;
	spinlock_release(&proc->fd_lock);

	return close(f);
}

int64_t sys_fcntl(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	int cmd = (int)args->rsi;
	uintptr_t arg = (uintptr_t)args->rdx;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);
	SYSCALL_REQUIRE_FD(fd);

	if (cmd == FCNTL_F_DUPFD || cmd == FCNTL_F_DUPFD_CLOEXEC) {
		int minfd = (int)arg;
		if (minfd < 0)
			return -EINVAL;

		spinlock_acquire(&proc->fd_lock);
		struct fileio *f = proc_fd_lookup_locked(proc, fd);
		if (!f) {
			spinlock_release(&proc->fd_lock);
			return -EBADF;
		}
		fio_retain(f);
		int newfd = proc_fd_alloc_from_locked(proc, f, minfd);
		if (newfd >= 0 && cmd == FCNTL_F_DUPFD_CLOEXEC)
			f->flags |= O_CLOEXEC;
		spinlock_release(&proc->fd_lock);

		if (newfd < 0) {
			close(f);
			return newfd;
		}
		return newfd;
	}

	spinlock_acquire(&proc->fd_lock);
	struct fileio *f = proc_fd_lookup_locked(proc, fd);
	if (!f) {
		spinlock_release(&proc->fd_lock);
		return -EBADF;
	}

	if (cmd == FCNTL_F_GETFL) {
		size_t flags = fcntl(f, F_GETFL, NULL);
		spinlock_release(&proc->fd_lock);
		return (int64_t)flags;
	}
	if (cmd == FCNTL_F_SETFL) {
		size_t flags = (size_t)arg;
		(void)fcntl(f, F_SETFL, &flags);
		spinlock_release(&proc->fd_lock);
		return 0;
	}
	if (cmd == FCNTL_F_GETFD) {
		int ret = (f->flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
		spinlock_release(&proc->fd_lock);
		return ret;
	}
	if (cmd == FCNTL_F_SETFD) {
		if ((int)arg & FD_CLOEXEC)
			f->flags |= O_CLOEXEC;
		else
			f->flags &= ~O_CLOEXEC;
		spinlock_release(&proc->fd_lock);
		return 0;
	}

	spinlock_release(&proc->fd_lock);
	return -EINVAL;
}

int64_t sys_openat(const syscall_args_t *args)
{
	int dirfd = (int)args->rdi;
	const char *path = (const char *)args->rsi;
	int flags = (int)args->rdx;
	mode_t mode = (mode_t)args->r10;

	SYSCALL_REQUIRE(path != NULL, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved = NULL;
	int r = syscall_resolve_user_path_at(proc, dirfd, path, &resolved);
	if (r != 0)
		return r;

	struct fileio *f = open(resolved, flags, mode);
	kfree(resolved);
	if (!f)
		return -ENOENT;

	spinlock_acquire(&proc->fd_lock);
	int fd = proc_fd_alloc_locked(proc, f);
	spinlock_release(&proc->fd_lock);

	if (fd < 0) {
		close(f);
		return fd;
	}

	return fd;
}

int64_t sys_faccessat(const syscall_args_t *args)
{
	int dirfd = (int)args->rdi;
	const char *path = (const char *)args->rsi;
	int mode = (int)args->rdx;
	int flags = (int)args->r10;

	SYSCALL_REQUIRE(path != NULL, -EFAULT);
	if (mode & ~(R_OK | W_OK | X_OK))
		return -EINVAL;
	if (flags & ~(AT_EACCESS | AT_SYMLINK_NOFOLLOW))
		return -EINVAL;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved = NULL;
	int r = syscall_resolve_user_path_at(proc, dirfd, path, &resolved);
	if (r != 0)
		return r;

	struct stat st;
	int ret = vfs_stat(resolved, &st);
	kfree(resolved);
	if (ret != 0)
		return -ENOENT;

	if ((mode & R_OK) && !(st.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)))
		return -EACCES;
	if ((mode & W_OK) && !(st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)))
		return -EACCES;
	if ((mode & X_OK) && !(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return -EACCES;

	return 0;
}

int64_t sys_readlink(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	char *buffer = (char *)args->rsi;
	size_t max_size = (size_t)args->rdx;
	size_t *length = (size_t *)args->r10;

	SYSCALL_REQUIRE(path != NULL, -EFAULT);
	SYSCALL_REQUIRE(length != NULL, -EFAULT);
	SYSCALL_REQUIRE(!(buffer == NULL && max_size != 0), -EFAULT);
	SYSCALL_REQUIRE(syscall_user_writable(length, sizeof(*length)) == 0,
					-EFAULT);
	if (max_size)
		SYSCALL_REQUIRE(syscall_user_writable(buffer, max_size) == 0, -EFAULT);

	if (!max_size) {
		size_t zero = 0;
		return syscall_copy_to_user(length, &zero, sizeof(zero));
	}

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved = NULL;
	int r = syscall_resolve_user_path(proc, path, &resolved);
	if (r != 0)
		return r;

	char *tmp = kmalloc(max_size + 1);
	if (!tmp) {
		kfree(resolved);
		return -ENOMEM;
	}

	int ret = vfs_readlink(resolved, tmp, max_size + 1);
	kfree(resolved);
	if (ret != 0) {
		kfree(tmp);
		return -EINVAL;
	}

	size_t out = strlen(tmp);
	if (out > max_size)
		out = max_size;

	ret = syscall_copy_to_user(buffer, tmp, out);
	kfree(tmp);
	if (ret != 0)
		return ret;

	return syscall_copy_to_user(length, &out, sizeof(out));
}

int64_t sys_mkdir(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	mode_t mode = (mode_t)args->rsi;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);
	SYSCALL_REQUIRE(path != NULL, -EFAULT);

	char *resolved = NULL;
	int r = syscall_resolve_user_path(proc, path, &resolved);
	if (r != 0)
		return r;

	mode &= ~proc->umask;
	int ret = vfs_mkdir(resolved, mode);
	kfree(resolved);

	return ret == 0 ? 0 : -ENOENT;
}

int64_t sys_mkdirat(const syscall_args_t *args)
{
	int dirfd = (int)args->rdi;
	const char *path = (const char *)args->rsi;
	mode_t mode = (mode_t)args->rdx;

	SYSCALL_REQUIRE(path != NULL, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved = NULL;
	int r = syscall_resolve_user_path_at(proc, dirfd, path, &resolved);
	if (r != 0)
		return r;

	mode &= ~proc->umask;
	int ret = vfs_mkdir(resolved, mode);
	kfree(resolved);

	return ret == 0 ? 0 : -ENOENT;
}

int64_t sys_unlinkat(const syscall_args_t *args)
{
	int dirfd = (int)args->rdi;
	const char *path = (const char *)args->rsi;
	int flags = (int)args->rdx;

	SYSCALL_REQUIRE(path != NULL, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved = NULL;
	int r = syscall_resolve_user_path_at(proc, dirfd, path, &resolved);
	if (r != 0)
		return r;

	int ret = 0;
	if (flags & AT_REMOVEDIR)
		ret = vfs_rmdir(resolved);
	else
		ret = vfs_remove(resolved);

	kfree(resolved);
	return ret == 0 ? 0 : -ENOENT;
}

int64_t sys_rename(const syscall_args_t *args)
{
	const char *old_path = (const char *)args->rdi;
	const char *new_path = (const char *)args->rsi;

	SYSCALL_REQUIRE(old_path != NULL, -EFAULT);
	SYSCALL_REQUIRE(new_path != NULL, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved_old = NULL;
	char *resolved_new = NULL;

	int r = syscall_resolve_user_path(proc, old_path, &resolved_old);
	if (r != 0)
		return r;

	r = syscall_resolve_user_path(proc, new_path, &resolved_new);
	if (r != 0) {
		kfree(resolved_old);
		return r;
	}

	struct stat st;
	if (vfs_stat(resolved_old, &st) != 0) {
		kfree(resolved_old);
		kfree(resolved_new);
		return -ENOENT;
	}

	if (mode_is_dir(st.st_mode)) {
		kfree(resolved_old);
		kfree(resolved_new);
		return -EPERM;
	}

	int ret = 0;
	if (mode_is_link(st.st_mode)) {
		char target[4096];
		int rl = vfs_readlink(resolved_old, target, sizeof(target));
		if (rl < 0) {
			ret = -EINVAL;
		} else {
			target[sizeof(target) - 1] = '\0';
			if (vfs_symlink(target, resolved_new) != 0)
				ret = -ENOENT;
		}
	} else {
		mode_t mode = st.st_mode & 0777;
		ret = copy_file_data(resolved_old, resolved_new, mode);
	}

	if (ret == 0 && vfs_remove(resolved_old) != 0)
		ret = -ENOENT;

	kfree(resolved_old);
	kfree(resolved_new);
	return ret;
}

int64_t sys_symlink(const syscall_args_t *args)
{
	const char *target = (const char *)args->rdi;
	const char *linkpath = (const char *)args->rsi;

	SYSCALL_REQUIRE(target != NULL, -EFAULT);
	SYSCALL_REQUIRE(linkpath != NULL, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *ktarget = NULL;
	char *resolved = NULL;
	int r = syscall_copy_string_from_user(target, &ktarget, SYSCALL_MAX_PATH);
	if (r != 0)
		return r;

	r = syscall_resolve_user_path(proc, linkpath, &resolved);
	if (r != 0) {
		kfree(ktarget);
		return r;
	}

	int ret = vfs_symlink(ktarget, resolved);
	kfree(ktarget);
	kfree(resolved);
	return ret == 0 ? 0 : -ENOENT;
}

int64_t sys_symlinkat(const syscall_args_t *args)
{
	const char *target = (const char *)args->rdi;
	int dirfd = (int)args->rsi;
	const char *linkpath = (const char *)args->rdx;

	SYSCALL_REQUIRE(target != NULL, -EFAULT);
	SYSCALL_REQUIRE(linkpath != NULL, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *ktarget = NULL;
	char *resolved = NULL;
	int r = syscall_copy_string_from_user(target, &ktarget, SYSCALL_MAX_PATH);
	if (r != 0)
		return r;

	r = syscall_resolve_user_path_at(proc, dirfd, linkpath, &resolved);
	if (r != 0) {
		kfree(ktarget);
		return r;
	}

	int ret = vfs_symlink(ktarget, resolved);
	kfree(ktarget);
	kfree(resolved);
	return ret == 0 ? 0 : -ENOENT;
}

int64_t sys_link(const syscall_args_t *args)
{
	const char *old_path = (const char *)args->rdi;
	const char *new_path = (const char *)args->rsi;

	SYSCALL_REQUIRE(old_path != NULL, -EFAULT);
	SYSCALL_REQUIRE(new_path != NULL, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved_old = NULL;
	char *resolved_new = NULL;
	int r = syscall_resolve_user_path(proc, old_path, &resolved_old);
	if (r != 0)
		return r;

	r = syscall_resolve_user_path(proc, new_path, &resolved_new);
	if (r != 0) {
		kfree(resolved_old);
		return r;
	}

	struct stat st;
	if (vfs_stat(resolved_old, &st) != 0) {
		kfree(resolved_old);
		kfree(resolved_new);
		return -ENOENT;
	}

	if (!mode_is_reg(st.st_mode)) {
		kfree(resolved_old);
		kfree(resolved_new);
		return -EPERM;
	}

	mode_t mode = st.st_mode & 0777;
	int ret = copy_file_data(resolved_old, resolved_new, mode);

	kfree(resolved_old);
	kfree(resolved_new);
	return ret;
}

int64_t sys_linkat(const syscall_args_t *args)
{
	int olddirfd = (int)args->rdi;
	const char *old_path = (const char *)args->rsi;
	int newdirfd = (int)args->rdx;
	const char *new_path = (const char *)args->r10;
	int flags = (int)args->r8;

	SYSCALL_REQUIRE(old_path != NULL, -EFAULT);
	SYSCALL_REQUIRE(new_path != NULL, -EFAULT);
	if (flags)
		return -ENOSYS;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved_old = NULL;
	char *resolved_new = NULL;

	int r =
		syscall_resolve_user_path_at(proc, olddirfd, old_path, &resolved_old);
	if (r != 0)
		return r;
	r = syscall_resolve_user_path_at(proc, newdirfd, new_path, &resolved_new);
	if (r != 0) {
		kfree(resolved_old);
		return r;
	}

	struct stat st;
	if (vfs_stat(resolved_old, &st) != 0) {
		kfree(resolved_old);
		kfree(resolved_new);
		return -ENOENT;
	}

	if (!mode_is_reg(st.st_mode)) {
		kfree(resolved_old);
		kfree(resolved_new);
		return -EPERM;
	}

	mode_t mode = st.st_mode & 0777;
	int ret = copy_file_data(resolved_old, resolved_new, mode);

	kfree(resolved_old);
	kfree(resolved_new);
	return ret;
}

int64_t sys_chmod(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	mode_t mode = (mode_t)args->rsi;

	SYSCALL_REQUIRE(path != NULL, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved = NULL;
	int r = syscall_resolve_user_path(proc, path, &resolved);
	if (r != 0)
		return r;

	struct stat st;
	if (vfs_stat(resolved, &st) != 0) {
		kfree(resolved);
		return -ENOENT;
	}

	st.st_mode = (st.st_mode & S_IFMT) | (mode & 07777);
	int ret = vfs_setstat(resolved, &st);
	kfree(resolved);
	return ret == 0 ? 0 : -EINVAL;
}

int64_t sys_umask(const syscall_args_t *args)
{
	mode_t mask = (mode_t)args->rdi;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	mode_t old = proc->umask;
	proc->umask = mask & 0777;
	return (int64_t)old;
}

int64_t sys_kill(const syscall_args_t *args)
{
	pid_t pid = (pid_t)args->rdi;
	int sig = (int)args->rsi;

	if (pid <= 0)
		return -ESRCH;

	struct pcb *caller = syscall_current_process();
	SYSCALL_REQUIRE_PROC(caller);

	pcb *target = proc_get_by_pid((uint32_t)pid);
	if (!target)
		return -ESRCH;

	if (sig == 0)
		return 0;

	if (caller->euid != 0 && caller->pid != target->pid &&
		caller->uid != target->uid && caller->euid != target->uid) {
		return -EPERM;
	}

	if (sig != 9 && sig != 15)
		return -ENOSYS;

	if (thread_current() && thread_current()->process == target) {
		thread_exit(thread_current(), 128 + sig);
		return 0;
	}

	if (target->parent_pid != caller->pid && caller->euid != 0)
		return -EPERM;

	proc_destroy(target);
	return 0;
}

int64_t sys_sigaction(const syscall_args_t *args)
{
	(void)args;
	return -ENOSYS;
}

int64_t sys_sleep(const syscall_args_t *args)
{
	uint64_t secs = (uint64_t)args->rdi;
	uint64_t nanos = (uint64_t)args->rsi;

	if (nanos >= 1000000000ULL)
		return -EINVAL;

	uint64_t ms = secs * 1000ULL + nanos / 1000000ULL;
	sleep_ms(ms);
	return 0;
}

int64_t sys_poll(const syscall_args_t *args)
{
	struct pollfd *user_fds = (struct pollfd *)args->rdi;
	size_t count = (size_t)args->rsi;
	int timeout = (int)args->rdx;
	int *num_events = (int *)args->r10;

	SYSCALL_REQUIRE(num_events != NULL, -EFAULT);
	SYSCALL_REQUIRE(count <= SYSCALL_MAX_POLLFDS, -EINVAL);
	SYSCALL_REQUIRE(!(user_fds == NULL && count != 0), -EFAULT);

	SYSCALL_REQUIRE(syscall_user_writable(num_events, sizeof(*num_events)) == 0,
					-EFAULT);
	if (count) {
		SYSCALL_REQUIRE(
			syscall_user_readable(user_fds, count * sizeof(struct pollfd)) == 0,
			-EFAULT);
		SYSCALL_REQUIRE(
			syscall_user_writable(user_fds, count * sizeof(struct pollfd)) == 0,
			-EFAULT);
	}

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	struct pollfd *kfds = NULL;
	if (count) {
		kfds = kmalloc(count * sizeof(*kfds));
		if (!kfds)
			return -ENOMEM;

		int r = syscall_copy_from_user(kfds, user_fds, count * sizeof(*kfds));
		if (r != 0) {
			kfree(kfds);
			return r;
		}
	}

	int ready_total = 0;

	for (int pass = 0; pass < 2; pass++) {
		ready_total = 0;

		for (size_t i = 0; i < count; i++) {
			struct pollfd *pfd = &kfds[i];
			pfd->revents = 0;

			if (pfd->fd < 0)
				continue;

			struct fileio *f = NULL;
			int r = syscall_fd_get(proc, pfd->fd, &f);
			if (r != 0) {
				pfd->revents |= POLLNVAL;
				ready_total++;
				continue;
			}

			if (f->flags & PIPE_READ_END) {
				struct pipe *p = (struct pipe *)f->private;
				if (p) {
					spinlock_acquire(&p->lock);
					if ((pfd->events & POLLIN) && p->used > 0)
						pfd->revents |= POLLIN;
					if (p->writers == 0)
						pfd->revents |= POLLHUP;
					spinlock_release(&p->lock);
				}
			} else if (f->flags & PIPE_WRITE_END) {
				struct pipe *p = (struct pipe *)f->private;
				if (p) {
					spinlock_acquire(&p->lock);
					if ((pfd->events & POLLOUT) && p->used < PIPE_BUFFER_SIZE)
						pfd->revents |= POLLOUT;
					if (p->readers == 0)
						pfd->revents |= POLLERR;
					spinlock_release(&p->lock);
				}
			} else {
				if (pfd->events & POLLIN)
					pfd->revents |= POLLIN;
				if (pfd->events & POLLOUT)
					pfd->revents |= POLLOUT;
			}

			if (pfd->revents)
				ready_total++;

			close(f);
		}

		if (ready_total > 0 || timeout == 0)
			break;

		if (timeout > 0 && pass == 0)
			sleep_ms((uint64_t)timeout);
		else
			break;
	}

	if (count) {
		int r = syscall_copy_to_user(user_fds, kfds, count * sizeof(*kfds));
		kfree(kfds);
		if (r != 0)
			return r;
	}

	return syscall_copy_to_user(num_events, &ready_total, sizeof(ready_total));
}

int64_t sys_dup(const syscall_args_t *args)
{
	int oldfd = (int)args->rdi;
	int flags = (int)args->rsi;

	if (flags & ~O_CLOEXEC)
		return -EINVAL;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	spinlock_acquire(&proc->fd_lock);
	struct fileio *f = proc_fd_lookup_locked(proc, oldfd);
	if (!f) {
		spinlock_release(&proc->fd_lock);
		return -EBADF;
	}
	fio_retain(f);
	int newfd = proc_fd_alloc_locked(proc, f);
	if (newfd >= 0 && (flags & O_CLOEXEC))
		f->flags |= O_CLOEXEC;
	spinlock_release(&proc->fd_lock);

	if (newfd < 0) {
		close(f);
		return newfd;
	}
	return newfd;
}

int64_t sys_dup2(const syscall_args_t *args)
{
	int oldfd = (int)args->rdi;
	int newfd = (int)args->rsi;
	int flags = (int)args->rdx;

	if (flags)
		return -EINVAL;
	if (newfd < 0 || newfd >= PROC_MAX_FDS)
		return -EBADF;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	spinlock_acquire(&proc->fd_lock);
	struct fileio *oldf = proc_fd_lookup_locked(proc, oldfd);
	if (!oldf) {
		spinlock_release(&proc->fd_lock);
		return -EBADF;
	}

	if (oldfd == newfd) {
		spinlock_release(&proc->fd_lock);
		return 0;
	}

	fio_retain(oldf);
	struct fileio *to_close = proc->fds[newfd];
	proc->fds[newfd] = oldf;
	spinlock_release(&proc->fd_lock);

	if (to_close)
		close(to_close);
	return 0;
}

int64_t sys_dup3(const syscall_args_t *args)
{
	int oldfd = (int)args->rdi;
	int newfd = (int)args->rsi;
	int flags = (int)args->rdx;

	if (oldfd == newfd)
		return -EINVAL;
	if (flags & ~O_CLOEXEC)
		return -EINVAL;
	if (newfd < 0 || newfd >= PROC_MAX_FDS)
		return -EBADF;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	spinlock_acquire(&proc->fd_lock);
	struct fileio *oldf = proc_fd_lookup_locked(proc, oldfd);
	if (!oldf) {
		spinlock_release(&proc->fd_lock);
		return -EBADF;
	}

	fio_retain(oldf);
	if (flags & O_CLOEXEC)
		oldf->flags |= O_CLOEXEC;
	else
		oldf->flags &= ~O_CLOEXEC;

	struct fileio *to_close = proc->fds[newfd];
	proc->fds[newfd] = oldf;
	spinlock_release(&proc->fd_lock);

	if (to_close)
		close(to_close);
	return 0;
}

int64_t sys_pipe(const syscall_args_t *args)
{
	int *fds = (int *)args->rdi;
	int flags = (int)args->rsi & O_CLOEXEC;

	SYSCALL_REQUIRE(fds != NULL, -EFAULT);
	SYSCALL_REQUIRE(syscall_user_writable(fds, sizeof(int) * 2) == 0, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	struct fileio *ends[2] = { NULL, NULL };
	if (pipe(ends) != 0)
		return -ENOMEM;

	if (flags & O_CLOEXEC) {
		ends[0]->flags |= O_CLOEXEC;
		ends[1]->flags |= O_CLOEXEC;
	}

	spinlock_acquire(&proc->fd_lock);
	int read_fd = proc_fd_alloc_locked(proc, ends[0]);
	if (read_fd < 0) {
		spinlock_release(&proc->fd_lock);
		close(ends[0]);
		close(ends[1]);
		return read_fd;
	}

	int write_fd = proc_fd_alloc_locked(proc, ends[1]);
	if (write_fd < 0) {
		proc->fds[read_fd] = NULL;
		spinlock_release(&proc->fd_lock);
		close(ends[0]);
		close(ends[1]);
		return write_fd;
	}
	spinlock_release(&proc->fd_lock);

	int out[2] = { read_fd, write_fd };
	return syscall_copy_to_user(fds, out, sizeof(out));
}

int64_t sys_stat(const syscall_args_t *args)
{
	int target = (int)args->rdi;
	int fd = (int)args->rsi;
	const char *path = (const char *)args->rdx;
	int flags = (int)args->r10;
	struct stat *st = (struct stat *)args->r8;

	(void)flags;

	SYSCALL_REQUIRE(st != NULL, -EFAULT);
	SYSCALL_REQUIRE(syscall_user_writable(st, sizeof(*st)) == 0, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	struct stat kst;

	if (target == FSFD_TARGET_FD) {
		struct fileio *f = NULL;
		int r = syscall_fd_get(proc, fd, &f);
		if (r != 0)
			return r;

		struct vnode *vnode = (struct vnode *)f->private;
		int ret = -EINVAL;
		if (vnode && vnode->ops && vnode->ops->getattr)
			ret = vnode->ops->getattr(vnode, &kst);

		close(f);
		if (ret != 0)
			return -EINVAL;
		return syscall_copy_to_user(st, &kst, sizeof(kst));
	}

	if (target != FSFD_TARGET_PATH && target != FSFD_TARGET_FD_PATH)
		return -EINVAL;
	if (!path)
		return -EFAULT;

	char *resolved = NULL;
	int r = syscall_resolve_user_path_at(
		proc, target == FSFD_TARGET_FD_PATH ? fd : AT_FDCWD, path, &resolved);
	if (r != 0)
		return r;

	int ret = vfs_stat(resolved, &kst);
	kfree(resolved);

	if (ret != 0)
		return -ENOENT;

	return syscall_copy_to_user(st, &kst, sizeof(kst));
}

int64_t sys_utimensat(const syscall_args_t *args)
{
	int dirfd = (int)args->rdi;
	const char *path = (const char *)args->rsi;
	const struct aurix_timespec *times_user =
		(const struct aurix_timespec *)args->rdx;
	int flags = (int)args->r10;

	if (flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH))
		return -EINVAL;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	struct aurix_timespec times_copy[2];
	const struct aurix_timespec *times = NULL;
	if (times_user) {
		int r =
			syscall_copy_from_user(times_copy, times_user, sizeof(times_copy));
		if (r != 0)
			return r;
		times = times_copy;
	}

	struct stat st;
	int stat_ret, set_ret;

	if (!path || ((flags & AT_EMPTY_PATH) && path && path[0] == '\0')) {
		struct fileio *f = NULL;
		int r = syscall_fd_get(proc, dirfd, &f);
		if (r != 0)
			return r;

		struct vnode *vnode = (struct vnode *)f->private;
		if (!vnode || !vnode->ops || !vnode->ops->getattr ||
			!vnode->ops->setattr) {
			close(f);
			return -EINVAL;
		}

		stat_ret = vnode->ops->getattr(vnode, &st);
		if (stat_ret != 0) {
			close(f);
			return -EINVAL;
		}

		apply_utimens(&st, times);
		set_ret = vnode->ops->setattr(vnode, &st);
		close(f);
		return set_ret == 0 ? 0 : -EINVAL;
	}

	char *resolved = NULL;
	int r = syscall_resolve_user_path_at(proc, dirfd, path, &resolved);
	if (r != 0)
		return r;

	stat_ret = vfs_stat(resolved, &st);
	if (stat_ret != 0) {
		kfree(resolved);
		return -ENOENT;
	}

	apply_utimens(&st, times);
	set_ret = vfs_setstat(resolved, &st);
	kfree(resolved);

	return set_ret == 0 ? 0 : -EINVAL;
}

int64_t sys_mount(const syscall_args_t *args)
{
	const char *source_user = (const char *)args->rdi;
	const char *target_user = (const char *)args->rsi;
	const char *fstype_user = (const char *)args->rdx;
	uint64_t flags = args->r10;
	void *data = (void *)args->r8;

	(void)source_user;

	SYSCALL_REQUIRE(target_user != NULL, -EFAULT);
	SYSCALL_REQUIRE(fstype_user != NULL, -EFAULT);
	SYSCALL_REQUIRE(flags == 0, -EINVAL);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);
	if (!syscall_is_privileged(proc))
		return -EPERM;

	char *target = NULL;
	char *fstype = NULL;

	int r =
		syscall_copy_string_from_user(target_user, &target, SYSCALL_MAX_PATH);
	if (r != 0)
		return r;

	r = syscall_copy_string_from_user(fstype_user, &fstype, 64);
	if (r != 0) {
		kfree(target);
		return r;
	}

	struct vfs *mounted = vfs_mount(NULL, fstype, target, data);
	kfree(target);
	kfree(fstype);

	return mounted ? 0 : -EINVAL;
}

int64_t sys_ioctl(const syscall_args_t *args)
{
	int fd = (int)args->rdi;
	int request = (int)args->rsi;
	void *arg = (void *)args->rdx;

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	struct fileio *f = NULL;
	int r = syscall_fd_get(proc, fd, &f);
	if (r != 0)
		return r;
	if (!f->private) {
		close(f);
		return -EBADF;
	}

	int ret = vfs_ioctl((struct vnode *)f->private, request, arg);
	close(f);

	if (ret == -1)
		return -ENOTTY;
	return ret;
}

int64_t sys_load_module(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	SYSCALL_REQUIRE(path != NULL, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);
	if (!syscall_is_privileged(proc))
		return -EPERM;

	char *resolved = NULL;
	int r = syscall_resolve_user_path(proc, path, &resolved);
	if (r != 0)
		return r;

	struct fileio *f = open(resolved, O_RDONLY, 0);
	kfree(resolved);
	if (!f)
		return -ENOENT;

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

	kfree(image);
	return 0;
}

int64_t sys_execve(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;
	const char *const *argv = (const char *const *)args->rsi;
	const char *const *envp = (const char *const *)args->rdx;

	SYSCALL_REQUIRE(path != NULL, -EFAULT);

	tcb *current = thread_current();
	struct pcb *cur = current ? current->process : NULL;
	SYSCALL_REQUIRE_PROC(cur);

	if (cur->threads && cur->threads->proc_next)
		return -EBUSY;

	char *resolved = NULL;
	char **argv_copy = NULL;
	char **envp_copy = NULL;
	char **shebang_argv = NULL;
	char *buf = NULL;
	char *script_path = NULL;
	char *exec_path = NULL;
	char *interp = NULL;
	char *interp_arg = NULL;
	char *interp_resolved = NULL;
	size_t argv_count = 0, envp_count = 0, shebang_argc = 0;
	size_t image_size = 0;

	int r = syscall_resolve_user_path(cur, path, &resolved);
	if (r != 0)
		return r;

	r = copy_user_string_vector(argv, EXEC_MAX_ARGS, &argv_copy, &argv_count);
	if (r != 0) {
		kfree(resolved);
		return r;
	}

	r = copy_user_string_vector(envp, EXEC_MAX_ENVP, &envp_copy, &envp_count);
	if (r != 0) {
		free_string_vector(argv_copy, argv_count);
		kfree(resolved);
		return r;
	}

	struct fileio *f = open(resolved, O_RDONLY, 0);
	if (!f) {
		free_string_vector(argv_copy, argv_count);
		free_string_vector(envp_copy, envp_count);
		kfree(resolved);
		return -ENOENT;
	}

	if (f->size == 0) {
		close(f);
		free_string_vector(argv_copy, argv_count);
		free_string_vector(envp_copy, envp_count);
		kfree(resolved);
		return -EINVAL;
	}

	image_size = f->size;
	buf = (char *)kmalloc(image_size);
	if (!buf) {
		close(f);
		free_string_vector(argv_copy, argv_count);
		free_string_vector(envp_copy, envp_count);
		kfree(resolved);
		return -ENOMEM;
	}

	if (read(f, image_size, buf) != image_size) {
		close(f);
		kfree(buf);
		free_string_vector(argv_copy, argv_count);
		free_string_vector(envp_copy, envp_count);
		kfree(resolved);
		return -EIO;
	}
	close(f);

	script_path = resolved;
	exec_path = resolved;

	int sb = parse_shebang(buf, image_size, &interp, &interp_arg);
	if (sb < 0) {
		free_string_vector(argv_copy, argv_count);
		free_string_vector(envp_copy, envp_count);
		kfree(buf);
		kfree(resolved);
		return sb;
	}

	if (sb > 0) {
		interp_resolved = syscall_resolve_path(cur, interp);
		if (!interp_resolved) {
			free_string_vector(argv_copy, argv_count);
			free_string_vector(envp_copy, envp_count);
			SYSCALL_KFREE_IF(interp);
			SYSCALL_KFREE_IF(interp_arg);
			kfree(buf);
			kfree(resolved);
			return -ENOMEM;
		}

		struct fileio *interp_file = open(interp_resolved, O_RDONLY, 0);
		if (!interp_file) {
			free_string_vector(argv_copy, argv_count);
			free_string_vector(envp_copy, envp_count);
			kfree(interp_resolved);
			SYSCALL_KFREE_IF(interp);
			SYSCALL_KFREE_IF(interp_arg);
			kfree(buf);
			kfree(resolved);
			return -ENOENT;
		}

		if (interp_file->size == 0) {
			close(interp_file);
			free_string_vector(argv_copy, argv_count);
			free_string_vector(envp_copy, envp_count);
			kfree(interp_resolved);
			SYSCALL_KFREE_IF(interp);
			SYSCALL_KFREE_IF(interp_arg);
			kfree(buf);
			kfree(resolved);
			return -EINVAL;
		}

		size_t interp_size = interp_file->size;
		char *interp_buf = (char *)kmalloc(interp_size);
		if (!interp_buf) {
			close(interp_file);
			free_string_vector(argv_copy, argv_count);
			free_string_vector(envp_copy, envp_count);
			kfree(interp_resolved);
			SYSCALL_KFREE_IF(interp);
			SYSCALL_KFREE_IF(interp_arg);
			kfree(buf);
			kfree(resolved);
			return -ENOMEM;
		}

		if (read(interp_file, interp_size, interp_buf) != interp_size) {
			close(interp_file);
			kfree(interp_buf);
			free_string_vector(argv_copy, argv_count);
			free_string_vector(envp_copy, envp_count);
			kfree(interp_resolved);
			SYSCALL_KFREE_IF(interp);
			SYSCALL_KFREE_IF(interp_arg);
			kfree(buf);
			kfree(resolved);
			return -EIO;
		}

		close(interp_file);
		kfree(buf);
		buf = interp_buf;
		image_size = interp_size;

		r = build_shebang_argv(interp_resolved, interp_arg, script_path,
							   argv_copy, argv_count, &shebang_argv,
							   &shebang_argc);
		if (r != 0) {
			free_string_vector(argv_copy, argv_count);
			free_string_vector(envp_copy, envp_count);
			kfree(interp_resolved);
			SYSCALL_KFREE_IF(interp);
			SYSCALL_KFREE_IF(interp_arg);
			kfree(buf);
			kfree(resolved);
			return r;
		}

		free_string_vector(argv_copy, argv_count);
		argv_copy = shebang_argv;
		argv_count = shebang_argc;
		exec_path = interp_resolved;
	}

	pagetable *new_pm = create_pagemap();
	if (!new_pm) {
		free_string_vector(argv_copy, argv_count);
		free_string_vector(envp_copy, envp_count);
		SYSCALL_KFREE_IF(interp);
		SYSCALL_KFREE_IF(interp_arg);
		if (exec_path != script_path)
			kfree(exec_path);
		kfree(script_path);
		kfree(buf);
		return -ENOMEM;
	}

	vctx_t *new_vctx = vinit(new_pm, 0x1000);
	if (!new_vctx) {
		destroy_pagemap(new_pm);
		free_string_vector(argv_copy, argv_count);
		free_string_vector(envp_copy, envp_count);
		SYSCALL_KFREE_IF(interp);
		SYSCALL_KFREE_IF(interp_arg);
		if (exec_path != script_path)
			kfree(exec_path);
		kfree(script_path);
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
			buf, exec_path, cur, (const char *const *)argv_copy, argv_count,
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
		SYSCALL_KFREE_IF(interp);
		SYSCALL_KFREE_IF(interp_arg);
		if (exec_path != script_path)
			kfree(exec_path);
		kfree(script_path);
		kfree(buf);
		return -EINVAL;
	}

	char *name_copy = kmalloc(strlen(exec_path) + 1);
	if (name_copy) {
		strcpy(name_copy, exec_path);
		if (cur->name)
			kfree((void *)cur->name);
		cur->name = (const char *)name_copy;
	}

	if (old_vctx)
		vdestroy(old_vctx);
	if (old_pm)
		destroy_pagemap(old_pm);
	if (old_image_elf)
		kfree(old_image_elf);

	SYSCALL_KFREE_IF(interp);
	SYSCALL_KFREE_IF(interp_arg);
	if (exec_path != script_path)
		kfree(exec_path);
	kfree(script_path);
	kfree(buf);
	free_string_vector(argv_copy, argv_count);
	free_string_vector(envp_copy, envp_count);

	exec_enter_current_user(current, cur, entry);
	__builtin_unreachable();
}

int64_t sys_exec(const syscall_args_t *args)
{
	const char *path = (const char *)args->rdi;

	syscall_args_t execve_args;
	memset(&execve_args, 0, sizeof(execve_args));
	execve_args.rdi = (uint64_t)(uintptr_t)path;
	execve_args.rsi = (uint64_t)(uintptr_t)NULL;
	execve_args.rdx = (uint64_t)(uintptr_t)NULL;

	return sys_execve(&execve_args);
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
	SYSCALL_REQUIRE_PROC(proc);
	if (!proc->vctx)
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
	SYSCALL_REQUIRE_PROC(proc);

	struct fileio *f = NULL;
	int r = syscall_fd_get(proc, fd, &f);
	if (r != 0)
		return r;

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
	SYSCALL_REQUIRE_PROC(proc);
	if (!proc->vctx)
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
	SYSCALL_REQUIRE_PROC(proc);
	if (!proc->vctx || !proc->pm)
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

	SYSCALL_REQUIRE(buf != NULL, -EINVAL);
	SYSCALL_REQUIRE(size != 0, -EINVAL);
	SYSCALL_REQUIRE(syscall_user_writable(buf, size) == 0, -EFAULT);

	struct pcb *proc = syscall_current_process();
	const char *cwd = (proc && proc->cwd) ? proc->cwd : "/";
	size_t len = strlen(cwd);
	if (len + 1 > size)
		return -ERANGE;

	return syscall_copy_to_user(buf, cwd, len + 1);
}

int64_t sys_fork(const syscall_args_t *args)
{
	SYSCALL_REQUIRE(args != NULL, -EINVAL);

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
	child->image_elf = NULL;
	child->image_size = 0;
	child->image_phys_base = 0;
	child->image_load_base = 0;
	child->image_link_base = 0;
	child->image_exec_size = 0;

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

	child->umask = parent->umask;
	child->uid = parent->uid;
	child->gid = parent->gid;
	child->euid = parent->euid;
	child->egid = parent->egid;

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
	SYSCALL_REQUIRE(path != NULL, -EFAULT);

	struct pcb *proc = syscall_current_process();
	SYSCALL_REQUIRE_PROC(proc);

	char *resolved = NULL;
	int r = syscall_resolve_user_path(proc, path, &resolved);
	if (r != 0)
		return r;

	struct vnode *vnode = NULL;
	int ret = vfs_lookup(resolved, &vnode);
	if (ret != 0) {
		kfree(resolved);
		return -ENOENT;
	}

	if (!vnode || vnode->vtype != VNODE_DIR) {
		if (vnode)
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
	SYSCALL_REQUIRE_PROC(parent);

	if (status)
		SYSCALL_REQUIRE(syscall_user_writable(status, sizeof(*status)) == 0,
						-EFAULT);

	struct pcb *child = proc_get_by_pid((uint32_t)pid);
	if (!child)
		return -ECHILD;
	if (child->parent_pid != parent->pid)
		return -ECHILD;

	while (!child->exited)
		sched_yield();

	if (status) {
		int st = (child->exit_code & 0xff) << 8;
		int r = syscall_copy_to_user(status, &st, sizeof(st));
		if (r != 0)
			return r;
	}

	proc_destroy(child);
	return pid;
}

int64_t sys_getpid(const syscall_args_t *args)
{
	(void)args;

	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -ESRCH;

	return (int64_t)proc->pid;
}

int64_t sys_getppid(const syscall_args_t *args)
{
	(void)args;

	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -ESRCH;

	return (int64_t)proc->parent_pid;
}

int64_t sys_getpgid(const syscall_args_t *args)
{
	pid_t pid = (pid_t)args->rdi;
	pid_t *pgid_out = (pid_t *)args->rsi;

	SYSCALL_REQUIRE(pgid_out != NULL, -EFAULT);
	SYSCALL_REQUIRE(syscall_user_writable(pgid_out, sizeof(*pgid_out)) == 0,
					-EFAULT);

	struct pcb *caller = syscall_current_process();
	SYSCALL_REQUIRE_PROC(caller);

	if (pid == 0)
		pid = caller->pid;
	if (pid < 0)
		return -EINVAL;

	struct pcb *target = proc_get_by_pid((uint32_t)pid);
	if (!target)
		return -ESRCH;

	pid_t pgid = (pid_t)target->pid;
	return syscall_copy_to_user(pgid_out, &pgid, sizeof(pgid));
}

int64_t sys_getuid(const syscall_args_t *args)
{
	(void)args;
	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -ESRCH;
	return (int64_t)proc->uid;
}

int64_t sys_geteuid(const syscall_args_t *args)
{
	(void)args;
	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -ESRCH;
	return (int64_t)proc->euid;
}

int64_t sys_getgid(const syscall_args_t *args)
{
	(void)args;
	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -ESRCH;
	return (int64_t)proc->gid;
}

int64_t sys_getegid(const syscall_args_t *args)
{
	(void)args;
	struct pcb *proc = syscall_current_process();
	if (!proc)
		return -ESRCH;
	return (int64_t)proc->egid;
}

int64_t sys_gettid(const syscall_args_t *args)
{
	(void)args;

	struct tcb *thr = thread_current();
	if (!thr)
		return -ESRCH;

	return (int64_t)thr->tid;
}

int64_t sys_clock_get(const syscall_args_t *args)
{
	int clock = (int)args->rdi;
	int64_t *secs = (int64_t *)args->rsi;
	int64_t *nanos = (int64_t *)args->rdx;

	SYSCALL_REQUIRE(secs != NULL, -EFAULT);
	SYSCALL_REQUIRE(nanos != NULL, -EFAULT);
	SYSCALL_REQUIRE(syscall_user_writable(secs, sizeof(*secs)) == 0, -EFAULT);
	SYSCALL_REQUIRE(syscall_user_writable(nanos, sizeof(*nanos)) == 0, -EFAULT);

	if (clock != CLOCK_REALTIME && clock != CLOCK_MONOTONIC)
		return -EINVAL;

	uint64_t ms = get_ms();
	int64_t s = (int64_t)(ms / 1000ull);
	int64_t ns = (int64_t)((ms % 1000ull) * 1000000ull);

	int r = syscall_copy_to_user(secs, &s, sizeof(s));
	if (r != 0)
		return r;
	return syscall_copy_to_user(nanos, &ns, sizeof(ns));
}

int64_t sys_set_fs_base(const syscall_args_t *args)
{
	uintptr_t base = (uintptr_t)args->rdi;
	if (!base)
		return -EINVAL;

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
	register_syscall(SYS_GETPID, sys_getpid, "getpid");
	register_syscall(SYS_GETUID, sys_getuid, "getuid");
	register_syscall(SYS_GETEUID, sys_geteuid, "geteuid");
	register_syscall(SYS_GETGID, sys_getgid, "getgid");
	register_syscall(SYS_GETEGID, sys_getegid, "getegid");
	register_syscall(SYS_GETPPID, sys_getppid, "getppid");
	register_syscall(SYS_GETTID, sys_gettid, "gettid");
	register_syscall(SYS_FCNTL, sys_fcntl, "fcntl");
	register_syscall(SYS_OPENAT, sys_openat, "openat");
	register_syscall(SYS_READLINK, sys_readlink, "readlink");
	register_syscall(SYS_POLL, sys_poll, "poll");
	register_syscall(SYS_DUP, sys_dup, "dup");
	register_syscall(SYS_DUP2, sys_dup2, "dup2");
	register_syscall(SYS_DUP3, sys_dup3, "dup3");
	register_syscall(SYS_PIPE, sys_pipe, "pipe");
	register_syscall(SYS_PIPE2, sys_pipe, "pipe2");
	register_syscall(SYS_MKDIR, sys_mkdir, "mkdir");
	register_syscall(SYS_MKDIRAT, sys_mkdirat, "mkdirat");
	register_syscall(SYS_UNLINKAT, sys_unlinkat, "unlinkat");
	register_syscall(SYS_RENAME, sys_rename, "rename");
	register_syscall(SYS_SYMLINK, sys_symlink, "symlink");
	register_syscall(SYS_SYMLINKAT, sys_symlinkat, "symlinkat");
	register_syscall(SYS_LINK, sys_link, "link");
	register_syscall(SYS_LINKAT, sys_linkat, "linkat");
	register_syscall(SYS_CHMOD, sys_chmod, "chmod");
	register_syscall(SYS_UMASK, sys_umask, "umask");
	register_syscall(SYS_KILL, sys_kill, "kill");
	register_syscall(SYS_SLEEP, sys_sleep, "sleep");
	register_syscall(SYS_SIGACTION, sys_sigaction, "sigaction");
	register_syscall(SYS_FACCESSAT, sys_faccessat, "faccessat");
	register_syscall(SYS_UTIMENSAT, sys_utimensat, "utimensat");
	register_syscall(SYS_GETPGID, sys_getpgid, "getpgid");
}