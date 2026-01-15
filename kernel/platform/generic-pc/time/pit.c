#include <arch/cpu/irq.h>
#include <arch/cpu/cpu.h>
#include <platform/time/pit.h>
#include <sys/sched.h>
#include <aurix.h>
#include <stdint.h>
#include <stdbool.h>

bool is_initialized = false;
uint32_t ticks = 0;

void tick(void *ctx)
{
    (void)ctx;
    ticks++;

    // TODO: Make a better system to manage callbacks every X ticks?
    sched_tick();
}

void pit_init(uint16_t freq)
{
    // if (is_initialized) {
        // info("returning early\n");
        // return;
    // }

    uint16_t div = PIT_CLOCK / freq;
    
    outb(PIT_COMMAND, 0x36); // mode 3, rw
    outb(PIT_COUNTER0, div & 0xFF);
    outb(PIT_COUNTER0, div >> 8);
    irq_install(0, tick, NULL);
    
    is_initialized = true;

    debug("PIT is now running at %uHz (divisor = %u).\n", PIT_CLOCK / div, div);
}
