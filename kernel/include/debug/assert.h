#ifndef _ASSERT_H
#define _ASSERT_H

#include <sys/panic.h>
#include <stdbool.h>

#define _assert_impl(cond, msg, ...)                      \
	do {                                                  \
		if (!(cond)) {                                    \
			if (sizeof((char[]){ 0, ##__VA_ARGS__ }) > 1) \
				kpanicf(NULL, msg, ##__VA_ARGS__);        \
			else                                          \
				kpanic(NULL, msg);                        \
		}                                                 \
	} while (0)

#define assert(cond, ...) _assert_impl(cond, __VA_ARGS__)

#endif // _ASSERT_H