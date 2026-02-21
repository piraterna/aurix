/*********************************************************************************/
/* Module Name:  serial.c */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
/*********************************************************************************/

#include <aurix/axapi.h>
#include <dev/device.h>
#include <dev/chrdev.h>
#include <dev/driver.h>
#include <serial16550.h>
#include <sys/aurix/mod.h>

#include <stddef.h>
#include <stdint.h>

#define COM1 0x3F8
#define COM2 0x2F8
#define COM3 0x3E8
#define COM4 0x2E8
#define COM5 0x5F8
#define COM6 0x4F8
#define COM7 0x5E8
#define COM8 0x4E8

#define SERIAL_PORTS(X)      \
	X(1, COM1, "/dev/ttyS0") \
	X(2, COM2, "/dev/ttyS1") \
	X(3, COM3, "/dev/ttyS2") \
	X(4, COM4, "/dev/ttyS3") \
	X(5, COM5, "/dev/ttyS4") \
	X(6, COM6, "/dev/ttyS5") \
	X(7, COM7, "/dev/ttyS6") \
	X(8, COM8, "/dev/ttyS7")

__attribute__((section(".aurix.mod"))) const struct axmod_info modinfo = {
	.name = "Serial16550",
	.desc = "Example 16550 UART driver module",
	.author = "Kevin Alavik",
	.mod_init = mod_init,
	.mod_exit = mod_exit,
};

struct serial_ctx {
	uint16_t base;
};

static int streq(const char *a, const char *b)
{
	if (!a || !b)
		return 0;
	while (*a && *b) {
		if (*a != *b)
			return 0;
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}

static uint8_t serial_tx_empty(uint16_t base)
{
	return ax_inb((uint16_t)(base + 5)) & 0x20;
}

static int serial_putc(uint16_t base, char c)
{
	for (uint32_t spins = 0; spins < 2000000u; spins++) {
		if (serial_tx_empty(base))
			break;
		if ((spins & 0xffu) == 0)
			ax_io_wait();
	}
	if (!serial_tx_empty(base))
		return -1;
	ax_outb(base, (uint8_t)c);
	return 0;
}

static void serial_init16550(uint16_t base)
{
	ax_outb((uint16_t)(base + 1), 0x00);
	ax_outb((uint16_t)(base + 3), 0x80);
	ax_outb((uint16_t)(base + 0), 0x03);
	ax_outb((uint16_t)(base + 1), 0x00);
	ax_outb((uint16_t)(base + 3), 0x03);
	ax_outb((uint16_t)(base + 2), 0xC7);
	ax_outb((uint16_t)(base + 4), 0x0B);
	ax_outb((uint16_t)(base + 4), 0x0F);
}

static int serial_port_present(uint16_t base)
{
	uint8_t lsr = ax_inb((uint16_t)(base + 5));
	if (lsr == 0xFF)
		return 0;

	ax_outb((uint16_t)(base + 7), 0x5A);
	if (ax_inb((uint16_t)(base + 7)) != 0x5A)
		return 0;

	ax_outb((uint16_t)(base + 7), 0xA5);
	if (ax_inb((uint16_t)(base + 7)) != 0xA5)
		return 0;

	return 1;
}

static int tty_write(void *ctx, const void *buf, size_t len)
{
	struct serial_ctx *c = (struct serial_ctx *)ctx;
	if (!c)
		return -1;
	const char *s = (const char *)buf;
	for (size_t i = 0; i < len; i++) {
		if (s[i] == '\r')
			continue;
		if (serial_putc(c->base, s[i]) != 0)
			return -1;
	}
	return (int)len;
}

static const struct chrdev_ops tty_ops = {
	.read = 0,
	.write = tty_write,
};

static int serial16550_probe(struct device *dev)
{
	if (!dev || !dev->name)
		return -1;

	uint16_t base = 0;
	const char *node = NULL;
	if (0) {
		;
	}
#define SERIAL_MATCH(n, basev, path)     \
	else if (streq(dev->name, "com" #n)) \
	{                                    \
		base = basev;                    \
		node = path;                     \
	}
	SERIAL_PORTS(SERIAL_MATCH)
	else
	{
		return -1;
	}
#undef SERIAL_MATCH

	if (!serial_port_present(base)) {
		kprintf("serial16550: %s is not present, skipping\n", dev->name);
		return -1;
	}

	struct serial_ctx *c = (struct serial_ctx *)kmalloc(sizeof(*c));
	if (!c)
		return -1;
	c->base = base;
	dev->driver_data = c;

	serial_init16550(base);
	if (devfs_register(node, &tty_ops, c) != 0) {
		dev->driver_data = 0;
		kfree(c);
		return -1;
	}
	return 0;
}

static struct driver serial16550_driver = {
	.name = "serial16550",
	.class_name = "serial",
	.probe = serial16550_probe,
	.remove = 0,
};

#define SERIAL_DEFINE_DEVICE(n, basev, path)   \
	static struct device serial_device_##n = { \
		.name = "com" #n,                      \
		.class_name = "serial",                \
		.driver_data = 0,                      \
		.bound_driver = 0,                     \
	};

SERIAL_PORTS(SERIAL_DEFINE_DEVICE)

#undef SERIAL_DEFINE_DEVICE

int mod_init(void)
{
#define SERIAL_REGISTER_DEVICE(n, basev, path) \
	ax_device_register(&serial_device_##n);
	SERIAL_PORTS(SERIAL_REGISTER_DEVICE)
#undef SERIAL_REGISTER_DEVICE

	ax_driver_register(&serial16550_driver);
	ax_driver_bind_all();

	for (;;)
		sched_yield();
}

void mod_exit(void)
{
}
