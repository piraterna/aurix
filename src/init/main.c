#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include <aurix/syscalls.h>

static int load_module_raw(const char *path)
{
	long result = raw_syscall6(SYS_LOAD_MODULE, (long)path, 0, 0, 0, 0, 0);
	if (result < 0 && result >= -4095) {
		errno = (int)-result;
		return -1;
	}

	errno = 0;
	return 0;
}

static void load_modules(void)
{
	int list = open("/sys/modules.list", O_RDONLY);
	if (list < 0) {
		printf("init: /sys/modules.list not found, skipping module load\n");
		return;
	}

	char buf[1024];
	ssize_t bytes = read(list, buf, sizeof(buf) - 1);
	close(list);

	if (bytes <= 0) {
		printf("init: /sys/modules.list is empty\n");
		return;
	}

	buf[bytes] = '\0';
	char *line = buf;

	for (ssize_t i = 0; i <= bytes; i++) {
		if (buf[i] != '\n' && buf[i] != '\0')
			continue;

		buf[i] = '\0';

		if (line[0] != '\0') {
			if (load_module_raw(line) < 0) {
				printf("init: failed to load module: %s (%s)\n", line,
					   strerror(errno));
			} else {
				printf("init: loaded module: %s\n", line);
				usleep(100 * 1000);
			}
		}

		line = &buf[i + 1];
	}
}

int main(void)
{
	printf("init: userspace starting\n");

	load_modules();

	printf("init: module load phase complete\n");
	printf("init: launching /bin/bash\n");

	struct passwd *pw = getpwuid(0);
	const char *home = (pw && pw->pw_dir) ? pw->pw_dir : "/";
	chdir(home);

	char *const argv[] = { "/bin/bash", "-li", NULL };

	char *const envp[] = { "PS1=\\$ ",			 "USER=root",
						   "HOSTNAME=unknown",	 "TERM=flanterm",
						   "HOME=/root",		 "SHELL=/bin/bash",
						   "PATH=/bin:/usr/bin", NULL };

	execve("/bin/bash", argv, envp);

	printf("init: execve /bin/bash failed: %s\n", strerror(errno));

	return 1;
}
