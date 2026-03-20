#ifndef _SYSCALL_H
#define _SYSCALL_H

typedef void *file_t;

void sys_exit(int code)
{
	__asm__ volatile("int $0x80" ::"a"(0), "D"(code));
	__builtin_unreachable();
}

file_t *sys_open(const char *path, int flags, int mode)
{
	file_t *file;
	__asm__ volatile("int $0x80"
					 : "=a"(file)
					 : "a"(1), "D"(path), "S"(flags), "d"(mode));
	return file;
}

int sys_read(file_t *file, void *buf, int count)
{
	int result;
	__asm__ volatile("int $0x80"
					 : "=a"(result)
					 : "a"(2), "D"(file), "S"(buf), "d"(count));
	return result;
}

int sys_write(file_t *file, const void *buf, int count)
{
	int result;
	__asm__ volatile("int $0x80"
					 : "=a"(result)
					 : "a"(3), "D"(file), "S"(buf), "d"(count));
	return result;
}

int sys_close(file_t *file)
{
	int result;
	__asm__ volatile("int $0x80" : "=a"(result) : "a"(4), "D"(file));
	return result;
}

#endif // _SYSCALL_H