/*********************************************************************************/
/* Module Name:  test.h */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#ifndef _TEST_TEST_H
#define _TEST_TEST_H

#include <config.h>
#include <stdint.h>

#define TEST_MAX_COUNT 16

typedef void (*test_func_t)(void);
typedef unsigned int uint32_t;

typedef struct {
	const char *name;
	test_func_t func;
	uint32_t pass_count;
	uint32_t fail_count;
	uint32_t current;
} test_case_t;

extern test_case_t test_suite[];
extern uint32_t test_count;

#ifdef CONFIG_BUILD_TESTS
#define TEST_ADD(fn)                                                        \
	do {                                                                    \
		if (test_count < TEST_MAX_COUNT) {                                  \
			test_suite[test_count].name = #fn;                              \
			test_suite[test_count].func = fn;                               \
			test_suite[test_count].pass_count = 0;                          \
			test_suite[test_count].fail_count = 0;                          \
			test_suite[test_count].current = 0;                             \
			test_count++;                                                   \
		} else {                                                            \
			warn("Test suite overflow: cannot add %s, max %d tests\n", #fn, \
				 TEST_MAX_COUNT);                                           \
		}                                                                   \
	} while (0)
#else
#define TEST_ADD(x)
#endif

#define TEST_EXPECT(condition)                      \
	do {                                            \
		if (!(condition)) {                         \
			test_suite[test_count - 1].current = 1; \
		}                                           \
	} while (0)

void test_run(uint32_t loop_count);

#endif /* _TEST_TEST_H */