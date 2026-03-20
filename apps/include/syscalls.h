#ifndef _SYSCALL_H
#define _SYSCALL_H

void sys_exit(int code)
{
	__asm__ volatile("int $0x80" ::"a"(0), "D"(code));
	__builtin_unreachable();
}

void *sys_open(const char *path, int flags, int mode)
{
	void *file;
	__asm__ volatile("int $0x80"
					 : "=a"(file)
					 : "a"(1), "D"(path), "S"(flags), "d"(mode));
	return file;
}

int sys_read(void *file, void *buf, int count)
{
	int result;
	__asm__ volatile("int $0x80"
					 : "=a"(result)
					 : "a"(2), "D"(file), "S"(buf), "d"(count));
	return result;
}

int sys_write(void *file, const void *buf, int count)
{
	int result;
	__asm__ volatile("int $0x80"
					 : "=a"(result)
					 : "a"(3), "D"(file), "S"(buf), "d"(count));
	return result;
}

int sys_close(void *file)
{
	int result;
	__asm__ volatile("int $0x80" : "=a"(result) : "a"(4), "D"(file));
	return result;
}

#endif // _SYSCALL_H