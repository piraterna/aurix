#include <test/heap_test.h>
#include <test/test.h>
#include <mm/heap.h>

void heap_test(void)
{
	char *test = kmalloc(1024);
	TEST_EXPECT(test != NULL);
	if (test) {
		*test = 'a';
		kfree(test);
	}
}