#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	char *msg = strdup("Hello, World!\n");
	if (!msg) {
		fprintf(stderr, "strdup failed\n");
		return 1;
	}

	printf("%s", msg);

	free(msg);
	return 0;
}
