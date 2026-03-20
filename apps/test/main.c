#include <syscalls.h>

int print(const char *str)
{
	file_t *f = sys_open("/dev/stdout", 0, 0);
	if (!f) {
		sys_exit(1);
	}

	int len = 0;
	while (str[len])
		len++;

	if (sys_write(f, str, len) < 0) {
		sys_close(f);
		sys_exit(1);
	}

	sys_close(f);
	return len;
}

void _start(void)
{
	print("Hello, World!\n");
	sys_exit(0);
	__builtin_unreachable();
}