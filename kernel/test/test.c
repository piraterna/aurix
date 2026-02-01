#include <test/test.h>
#include <aurix.h>
#include <stdint.h>

test_case_t test_suite[TEST_MAX_COUNT];
uint32_t test_count = 0;

void test_run(uint32_t loop_count)
{
	(void)loop_count;
#ifdef CONFIG_BUILD_TESTS
	debug("Starting test suite with %u loops, %u tests registered\n",
		  loop_count, test_count);
	if (test_count == 0) {
		warn("No tests registered to run\n");
		return;
	}
	for (uint32_t i = 0; i < test_count; i++) {
		test_suite[i].pass_count = 0;
		test_suite[i].fail_count = 0;
		test("Running test: %s\n", test_suite[i].name);
		for (uint32_t j = 0; j < loop_count; j++) {
			test_suite[i].current = 0;
			test_suite[i].func();
			if (test_suite[i].current == 0) {
				test_suite[i].pass_count++;
				success("Test %s (run %u): Passed\n", test_suite[i].name,
						j + 1);
			} else {
				test_suite[i].fail_count++;
				error("Test %s (run %u): Failed\n", test_suite[i].name, j + 1);
			}
		}
		test("Test %s: %u passes, %u fails\n", test_suite[i].name,
			 test_suite[i].pass_count, test_suite[i].fail_count);
	}
	test_count = 0;
	debug("Test suite completed and cleared\n");
#endif
}
