#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	printf("Hello, World from %s!\n", "mlibc");
	void *a = malloc(sizeof(char));
	if (!a) {
		printf("error: Failed to allocate using malloc!\n");
		return 1;
	}
	printf("allocated some memory at %p\n", a);
	return 0;
}