#include <syscalls.h>

int print(const char *str)
{
	int len = 0;
	while (str[len])
		len++;

	if (sys_write(1, str, len) < 0) {
		sys_exit(1);
	}

	return len;
}

void _start(void)
{
	print("Hello, World!\n");
	sys_exit(0);
	__builtin_unreachable();
}
