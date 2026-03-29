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
			}
		}

		line = &buf[i + 1];
	}
}

static const char *resolve_shell(const struct passwd *pw)
{
	if (pw && pw->pw_shell && pw->pw_shell[0] != '\0') {
		return pw->pw_shell;
	}

	return "/bin/sh";
}

static const char *resolve_home(const struct passwd *pw)
{
	if (pw && pw->pw_dir && pw->pw_dir[0] != '\0') {
		return pw->pw_dir;
	}

	return "/";
}

static const char *resolve_user(const struct passwd *pw)
{
	if (pw && pw->pw_name && pw->pw_name[0] != '\0') {
		return pw->pw_name;
	}

	return "root";
}

static void setup_shell_env(const struct passwd *pw, const char *shell,
							const char *home, const char *user)
{
	char hostname[256];

	if (setenv("HOME", home, 1) < 0)
		printf("init: failed to set HOME: %s\n", strerror(errno));

	if (setenv("USER", user, 1) < 0)
		printf("init: failed to set USER: %s\n", strerror(errno));

	if (setenv("LOGNAME", user, 1) < 0)
		printf("init: failed to set LOGNAME: %s\n", strerror(errno));

	if (setenv("SHELL", shell, 1) < 0)
		printf("init: failed to set SHELL: %s\n", strerror(errno));

	if (setenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin", 0) < 0)
		printf("init: failed to set PATH: %s\n", strerror(errno));

	if (setenv("TERM", "flanterm", 0) < 0)
		printf("init: failed to set TERM: %s\n", strerror(errno));

	if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
		hostname[sizeof(hostname) - 1] = '\0';
		if (hostname[0] != '\0' && setenv("HOSTNAME", hostname, 1) < 0)
			printf("init: failed to set HOSTNAME: %s\n", strerror(errno));
	}

	if (pw == NULL)
		printf("init: no passwd entry for uid %ld, using fallback defaults\n",
			   (long)getuid());
}

static void exec_shell(const struct passwd *pw)
{
	const char *shell = resolve_shell(pw);
	const char *home = resolve_home(pw);
	const char *user = resolve_user(pw);
	char *const argv[] = { (char *)shell, "-c",
						   ". /etc/profile; exec \"$0\" -i",
						   (char *)shell, NULL };
	char *const fallback_argv[] = {
		"/bin/sh", "-c", ". /etc/profile; exec \"$0\" -i",
		"/bin/sh", NULL
	};

	setup_shell_env(pw, shell, home, user);

	if (chdir(home) < 0) {
		printf("init: failed to chdir to %s: %s\n", home, strerror(errno));
		if (chdir("/") < 0)
			printf("init: failed to chdir to /: %s\n", strerror(errno));
	}

	printf("init: launching %s\n", shell);
	execv(shell, argv);

	printf("init: execv %s failed: %s\n", shell, strerror(errno));

	if (strcmp(shell, "/bin/sh") != 0) {
		printf("init: falling back to /bin/sh\n");
		execv("/bin/sh", fallback_argv);
		printf("init: execv /bin/sh failed: %s\n", strerror(errno));
	}
}

int main(void)
{
	printf("init: userspace starting\n");

	load_modules();

	printf("init: module load phase complete\n");

	struct passwd *pw = getpwuid(getuid());
	exec_shell(pw);

	return 1;
}
