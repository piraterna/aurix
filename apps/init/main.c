#include <syscalls.h>

static int str_len(const char *s)
{
	int len = 0;
	while (s && s[len]) {
		len++;
	}
	return len;
}

static void print_line(file_t *out, const char *msg)
{
	if (!out || !msg) {
		return;
	}

	sys_write(out, msg, str_len(msg));
}

static void load_modules(file_t *out)
{
	file_t *list = sys_open("/sys/modules.list", 0, 0);
	if (!list) {
		print_line(out,
				   "init: /sys/modules.list not found, skipping module load\n");
		return;
	}

	char buf[1024];
	int bytes = sys_read(list, buf, sizeof(buf) - 1);
	sys_close(list);

	if (bytes <= 0) {
		print_line(out, "init: /sys/modules.list is empty\n");
		return;
	}

	buf[bytes] = '\0';
	char *line = buf;

	for (int i = 0; i <= bytes; i++) {
		if (buf[i] != '\n' && buf[i] != '\0') {
			continue;
		}

		buf[i] = '\0';
		if (line[0] != '\0') {
			if (sys_load_module(line) < 0) {
				print_line(out, "init: failed to load module: ");
				print_line(out, line);
				print_line(out, "\n");
			} else {
				print_line(out, "init: loaded module: ");
				print_line(out, line);
				print_line(out, "\n");
			}
		}

		line = &buf[i + 1];
	}
}

void _start(void)
{
	const char *msg = "init: userspace starting\n";

	file_t *f = sys_open("/dev/stdout", 0, 0);
	if (!f) {
		sys_exit(1);
	}

	if (sys_write(f, msg, str_len(msg)) < 0) {
		sys_close(f);
		sys_exit(1);
	}

	load_modules(f);
	print_line(f, "init: module load phase complete\n");
	print_line(f, "init: launching /bin/test\n");
	sys_exec("/bin/test");

	sys_close(f);
	sys_exit(0);
	__builtin_unreachable();
}
