#ifndef _PLATFORM_TIME_PIT_H
#define _PLATFORM_TIME_PIT_H

#include <stdint.h>

#define PIT_CLOCK 1193182
#define PIT_COMMAND 0x43
#define PIT_COUNTER0 0x40

void pit_init(uint16_t freq);

#endif /* _PLATFORM_TIME_PIT_H */