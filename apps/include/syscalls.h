#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <errno.h>

typedef void *file_t;

static inline long syscall_ret(long value)
{
	if (value < 0 && value >= -4095) {
		errno = (int)-value;
		return -1;
	}

	errno = 0;
	return value;
}

void sys_exit(int code)
{
	__asm__ volatile("int $0x80" ::"a"(0), "D"(code));
	__builtin_unreachable();
}

file_t *sys_open(const char *path, int flags, int mode)
{
	long ret;
	__asm__ volatile("int $0x80"
					 : "=a"(ret)
					 : "a"(1), "D"(path), "S"(flags), "d"(mode));

	if (ret < 0 && ret >= -4095) {
		errno = (int)-ret;
		return (file_t *)0;
	}

	errno = 0;
	return (file_t *)ret;
}

int sys_read(file_t *file, void *buf, int count)
{
	int result;
	__asm__ volatile("int $0x80"
					 : "=a"(result)
					 : "a"(2), "D"(file), "S"(buf), "d"(count));

	return (int)syscall_ret(result);
}

int sys_write(file_t *file, const void *buf, int count)
{
	int result;
	__asm__ volatile("int $0x80"
					 : "=a"(result)
					 : "a"(3), "D"(file), "S"(buf), "d"(count));

	return (int)syscall_ret(result);
}

int sys_close(file_t *file)
{
	int result;
	__asm__ volatile("int $0x80" : "=a"(result) : "a"(4), "D"(file));

	return (int)syscall_ret(result);
}

int sys_mount(const char *source, const char *target, const char *fstype,
			  unsigned long flags, void *data)
{
	long result;
	register unsigned long r10 __asm__("r10") = flags;
	register void *r8 __asm__("r8") = data;

	__asm__ volatile("int $0x80"
					 : "=a"(result)
					 : "a"(5), "D"(source), "S"(target), "d"(fstype), "r"(r10),
					   "r"(r8)
					 : "memory");

	return (int)syscall_ret(result);
}

int sys_ioctl(file_t *file, int request, void *arg)
{
	long result;
	__asm__ volatile("int $0x80"
					 : "=a"(result)
					 : "a"(6), "D"(file), "S"(request), "d"(arg));

	return (int)syscall_ret(result);
}

int sys_load_module(const char *path)
{
	long result;
	__asm__ volatile("int $0x80" : "=a"(result) : "a"(7), "D"(path));

	return (int)syscall_ret(result);
}

#endif // _SYSCALL_H
