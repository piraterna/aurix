#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#include "builtins.h"

extern char **environ;

static int builtin_export(int argc, char *argv[], bool *should_exit);
static int builtin_unset(int argc, char *argv[], bool *should_exit);
static int builtin_cd(int argc, char *argv[], bool *should_exit);

#define BUILTIN_LIST          \
	X(export, builtin_export) \
	X(unset, builtin_unset)   \
	X(cd, builtin_cd)

static const struct builtin builtin_table[] = {
#define X(name, fn) { #name, fn },
	BUILTIN_LIST
#undef X
};

static const size_t builtin_table_count =
	sizeof(builtin_table) / sizeof(builtin_table[0]);

size_t builtin_count(void)
{
	return builtin_table_count;
}

const struct builtin *builtin_lookup(const char *name)
{
	if (!name)
		return NULL;

	for (size_t i = 0; i < builtin_table_count; i++) {
		if (strcmp(name, builtin_table[i].name) == 0)
			return &builtin_table[i];
	}

	return NULL;
}

static int builtin_cd(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;
	const char *path = NULL;

	if (argc < 2) {
		path = getenv("HOME");
		if (path == NULL) {
			fprintf(stderr, "cd: HOME not set\n");
			return 1;
		}
	} else {
		path = argv[1];
	}

	if (chdir(path) != 0) {
		fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
		return 1;
	}

	return 0;
}

static int builtin_export(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 2) {
		fprintf(stderr, "export: usage: export NAME=VALUE\n");
		return 1;
	}

	char *eq = strchr(argv[1], '=');
	if (eq == NULL) {
		fprintf(stderr, "export: expected NAME=VALUE\n");
		return 1;
	}

	*eq = '\0';
	const char *name = argv[1];
	const char *value = eq + 1;

	if (name[0] == '\0') {
		fprintf(stderr, "export: empty variable name\n");
		return 1;
	}

	if (setenv(name, value, 1) != 0) {
		fprintf(stderr, "export: %s\n", strerror(errno));
		return 1;
	}

	return 0;
}

static int builtin_unset(int argc, char *argv[], bool *should_exit)
{
	(void)should_exit;

	if (argc < 2) {
		fprintf(stderr, "unset: usage: unset NAME\n");
		return 1;
	}

	if (unsetenv(argv[1]) != 0) {
		fprintf(stderr, "unset: %s\n", strerror(errno));
		return 1;
	}

	return 0;
}
