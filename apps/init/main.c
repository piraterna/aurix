void _start(void)
{
	volatile int *ptr = (int *)0xDEADBEEF;
	*ptr = 69;
	for (;;)
		;
}