#ifndef SH_BUILTINS_H
#define SH_BUILTINS_H

#include <stdbool.h>
#include <stddef.h>

struct builtin {
	const char *name;
	int (*handler)(int argc, char *argv[], bool *should_exit);
};

const struct builtin *builtin_lookup(const char *name);
size_t builtin_count(void);

#endif
