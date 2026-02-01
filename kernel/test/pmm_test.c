#include <mm/pmm.h>
#include <test/pmm_test.h>
#include <test/test.h>

void pmm_test(void)
{
	char *test = palloc(1);
	TEST_EXPECT(test != NULL);
	if (test)
		pfree(test, 1);
}