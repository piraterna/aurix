#include <syscalls.h>

void _start(void)
{
	const char *msg = "Hello from init! we are just going to exit rn.\n";

	void *f = sys_open("/dev/stdout", 0, 0);
	if (!f) {
		sys_exit(1);
	}

	if (sys_write(f, msg, 47) < 0) {
		sys_close(f);
		sys_exit(1);
	}

	sys_close(f);
	sys_exit(0);
	__builtin_unreachable();
}