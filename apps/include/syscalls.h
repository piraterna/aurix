#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <errno.h>
#include <stddef.h>

enum {
	SYS_EXIT = 0,
	SYS_OPEN = 1,
	SYS_READ = 2,
	SYS_WRITE = 3,
	SYS_CLOSE = 4,
	SYS_MOUNT = 5,
	SYS_IOCTL = 6,
	SYS_LOAD_MODULE = 7,
	SYS_EXEC = 8,
	SYS_MMAP = 9,
	SYS_LSEEK = 10,
	SYS_MUNMAP = 11,
	SYS_CLOCK_GET = 12,
	SYS_SET_FS_BASE = 13,
};

#define PROT_READ 0x01
#define PROT_WRITE 0x02
#define PROT_EXEC 0x04

#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANON 0x20
#define MAP_ANONYMOUS MAP_ANON

typedef int file_t;

static inline long syscall_ret(long value)
{
	if (value < 0 && value >= -4095) {
		errno = (int)-value;
		return -1;
	}

	errno = 0;
	return value;
}

static inline long raw_syscall6(long id, long a1, long a2, long a3, long a4,
								long a5, long a6)
{
	long ret;
	register long r10 __asm__("r10") = a4;
	register long r8 __asm__("r8") = a5;
	register long r9 __asm__("r9") = a6;

	__asm__ volatile("syscall"
					 : "=a"(ret)
					 : "a"(id), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8),
					   "r"(r9)
					 : "rcx", "r11", "memory");

	return ret;
}

static inline void sys_exit(int code)
{
	raw_syscall6(SYS_EXIT, code, 0, 0, 0, 0, 0);
	__builtin_unreachable();
}

static inline int sys_open(const char *path, int flags, int mode)
{
	long ret = raw_syscall6(SYS_OPEN, (long)path, flags, mode, 0, 0, 0);
	if (ret < 0 && ret >= -4095) {
		errno = (int)-ret;
		return -1;
	}
	errno = 0;
	return (int)ret;
}

static inline int sys_read(file_t file, void *buf, int count)
{
	long result = raw_syscall6(SYS_READ, (long)file, (long)buf, count, 0, 0, 0);
	return (int)syscall_ret(result);
}

static inline int sys_write(file_t file, const void *buf, int count)
{
	long result =
		raw_syscall6(SYS_WRITE, (long)file, (long)buf, count, 0, 0, 0);
	return (int)syscall_ret(result);
}

static inline int sys_close(file_t file)
{
	long result = raw_syscall6(SYS_CLOSE, (long)file, 0, 0, 0, 0, 0);
	return (int)syscall_ret(result);
}

static inline int sys_mount(const char *source, const char *target,
							const char *fstype, unsigned long flags, void *data)
{
	long result = raw_syscall6(SYS_MOUNT, (long)source, (long)target,
							   (long)fstype, flags, (long)data, 0);
	return (int)syscall_ret(result);
}

static inline int sys_ioctl(file_t file, int request, void *arg)
{
	long result =
		raw_syscall6(SYS_IOCTL, (long)file, request, (long)arg, 0, 0, 0);
	return (int)syscall_ret(result);
}

static inline int sys_load_module(const char *path)
{
	long result = raw_syscall6(SYS_LOAD_MODULE, (long)path, 0, 0, 0, 0, 0);
	return (int)syscall_ret(result);
}

static inline int sys_exec(const char *path)
{
	long result = raw_syscall6(SYS_EXEC, (long)path, 0, 0, 0, 0, 0);
	return (int)syscall_ret(result);
}

static inline void *sys_mmap(void *addr, size_t length, int prot, int flags,
							 int fd, size_t offset)
{
	long result = raw_syscall6(SYS_MMAP, (long)addr, (long)length, prot, flags,
							   fd, offset);
	if (result < 0 && result >= -4095) {
		errno = (int)-result;
		return (void *)-1;
	}

	errno = 0;
	return (void *)result;
}

static inline long sys_lseek(file_t file, long offset, int whence)
{
	long result = raw_syscall6(SYS_LSEEK, (long)file, offset, whence, 0, 0, 0);
	return syscall_ret(result);
}

static inline int sys_munmap(void *addr, size_t length)
{
	long result =
		raw_syscall6(SYS_MUNMAP, (long)addr, (long)length, 0, 0, 0, 0);
	return (int)syscall_ret(result);
}

static inline int sys_clock_get(int clock, long *secs, long *nanos)
{
	long result =
		raw_syscall6(SYS_CLOCK_GET, clock, (long)secs, (long)nanos, 0, 0, 0);
	return (int)syscall_ret(result);
}

static inline int sys_set_fs_base(void *base)
{
	long result = raw_syscall6(SYS_SET_FS_BASE, (long)base, 0, 0, 0, 0, 0);
	return (int)syscall_ret(result);
}

#endif // _SYSCALL_H
