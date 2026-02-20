#ifndef _SYS_PANIC_H
#define _SYS_PANIC_H

struct interrupt_frame;

__attribute__((noreturn)) void kpanic(const struct interrupt_frame *frame,
									  const char *reason);

#endif
