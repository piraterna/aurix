void _start(void)
{
	const char *msg = "Hello, world!\n";
	__asm__ volatile("int $0x80" ::"a"(1), "D"(msg), "S"(14)); // sys_print
	__asm__ volatile("int $0x80" ::"a"(0), "D"(0)); // sys_exit
	__builtin_unreachable();
}