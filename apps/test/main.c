#include <syscalls.h>

void _start(void)
{
	const char *msg = "Hello, world!\n";

	void *f = sys_open("/dev/stdout", 0, 0);
	if (!f) {
		sys_exit(1);
	}

	if (sys_write(f, msg, 14) < 0) {
		sys_close(f);
		sys_exit(1);
	}

	sys_close(f);
	sys_exit(0);
	__builtin_unreachable();
}