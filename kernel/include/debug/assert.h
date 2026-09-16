#ifndef _ASSERT_H
#define _ASSERT_H

#include <sys/panic.h>
#include <stdbool.h>

#define _assert_choose(_1, _2, _3, _4, _5, _6, NAME, ...) NAME

#define assert(cond, ...)                                                   \
	do {                                                                    \
		if (!!(cond)) {                                                      \
			_assert_choose(__VA_ARGS__, _assert_fail_msg, _assert_fail_msg, \
						   _assert_fail_msg, _assert_fail_msg,              \
						   _assert_fail_msg, _assert_fail_nomsg)(           \
				#cond, __FILE__, __LINE__, __func__, ##__VA_ARGS__);        \
		}                                                                   \
	} while (0)

#endif