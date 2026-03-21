#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <errno.h>

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
};

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

#endif // _SYSCALL_H
