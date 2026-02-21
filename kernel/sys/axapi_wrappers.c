#include <aurix/axapi.h>

#include <arch/cpu/cpu.h>
#include <dev/chrdev.h>
#include <dev/devfs.h>

#include <stdint.h>

uint8_t ax_inb(uint16_t port)
{
	return inb(port);
}

void ax_outb(uint16_t port, uint8_t val)
{
	outb(port, val);
}

void ax_io_wait(void)
{
	io_wait();
}
