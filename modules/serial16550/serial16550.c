/*********************************************************************************/
/* Module Name:  driver.c */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#include <aurix/axapi.h>
#include <dev/device.h>
#include <dev/driver.h>
#include <sys/aurix/mod.h>
#include <serial16550.h>

#define LSR_DATA_READY 0x01
#define LSR_THR_EMPTY 0x20
#define SERIAL_BASE_PATH "/dev/raw/serial/"

static const uint16_t COM_BASES[8] = { 0x3F8, 0x2F8, 0x3E8, 0x2E8,
									   0x5F8, 0x4F8, 0x5E8, 0x4E8 };

struct serial_ctx {
	uint16_t base;
};

static struct serial_ctx serial_ctxs[8];

static int serial_read(struct device *dev, void *buf, uint64_t len);
static int serial_write(struct device *dev, const void *buf, uint64_t len);

static struct device_ops serial_ops = {
	.read = serial_read,
	.write = serial_write,
	.ioctl = 0,
};

static uint8_t serial_tx_empty(uint16_t base)
{
	return ax_inb(base + 5) & LSR_THR_EMPTY;
}

static int serial_putc(uint16_t base, char c)
{
	for (uint32_t spins = 0; spins < 2000000u; spins++) {
		if (serial_tx_empty(base))
			break;
		if ((spins & 0xff) == 0)
			ax_io_wait();
	}
	if (!serial_tx_empty(base))
		return -1;
	ax_outb(base, (uint8_t)c);
	return 0;
}

static void serial_init_port(uint16_t base)
{
	ax_outb(base + 1, 0x00); // disable interrupts
	ax_outb(base + 3, 0x80); // set DLAB
	ax_outb(base + 0, 0x03); // baud divisor low
	ax_outb(base + 1, 0x00); // baud divisor high
	ax_outb(base + 3, 0x03); // 8N1
	ax_outb(base + 2, 0xC7); // FIFO
	ax_outb(base + 4, 0x0B); // modem control
	ax_outb(base + 4, 0x0F); // modem control
}

static int serial_port_present(uint16_t base)
{
	uint8_t lsr = ax_inb(base + 5);
	if (lsr == 0xFF)
		return 0;
	ax_outb(base + 7, 0x5A);
	if (ax_inb(base + 7) != 0x5A)
		return 0;
	ax_outb(base + 7, 0xA5);
	if (ax_inb(base + 7) != 0xA5)
		return 0;
	return 1;
}

static int serial_read(struct device *dev, void *buf, uint64_t len)
{
	if (!dev || !dev->driver_data || !buf)
		return -1;
	struct serial_ctx *ctx = dev->driver_data;
	uint8_t *dst = buf;
	for (uint64_t i = 0; i < len; i++) {
		while (!(ax_inb(ctx->base + 5) & LSR_DATA_READY)) {
		}
		dst[i] = ax_inb(ctx->base + 0);
	}
	return (int)len;
}

static int serial_write(struct device *dev, const void *buf, uint64_t len)
{
	if (!dev || !dev->driver_data || !buf)
		return -1;
	struct serial_ctx *ctx = dev->driver_data;
	const char *src = buf;
	for (uint64_t i = 0; i < len; i++) {
		if (src[i] == '\r')
			continue;
		if (serial_putc(ctx->base, src[i]) != 0)
			return -1;
	}
	return (int)len;
}

static int serial_probe(struct device *dev)
{
	if (!dev || !dev->name)
		return -1;

	int idx = -1;
	for (int i = 0; i < 8; i++) {
		char buf[5];
		snprintf(buf, sizeof(buf), "com%d", i + 1);
		if (streq(dev->name, buf)) {
			idx = i;
			break;
		}
	}

	if (idx < 0) {
		kprintf("serial16550: unknown device %s\n", dev->name);
		return -1;
	}

	uint16_t base = COM_BASES[idx];
	if (!serial_port_present(base)) {
		kprintf("serial16550: %s not present, skipping\n", dev->name);
		return -1;
	}

	dev->driver_data = &serial_ctxs[idx];
	dev->ops = &serial_ops;
	serial_ctxs[idx].base = base;

	serial_init_port(base);
	kprintf("serial16550: %s initialized at 0x%X\n", dev->name, base);
	return 0;
}

static struct driver serial_driver = {
	.name = "serial16550",
	.class_name = "serial",
	.probe = serial_probe,
	.remove = 0,
};

__attribute__((section(".aurix.mod"))) const struct axmod_info modinfo = {
	.name = "Serial16550",
	.desc = "16550 UART driver module",
	.author = "Kevin Alavik",
	.mod_init = mod_init,
	.mod_exit = mod_exit,
};

int mod_init(void)
{
	for (int i = 0; i < 8; i++) {
		uint16_t base = COM_BASES[i];
		if (!serial_port_present(base))
			continue;

		struct device *dev = kmalloc(sizeof(*dev));
		if (!dev)
			continue;

		memset(dev, 0, sizeof(*dev));

		dev->name = kmalloc(6);
		snprintf((char *)dev->name, 6, "com%d", i + 1);

		dev->class_name = "serial";

		dev->dev_node_path = kmalloc(strlen(SERIAL_BASE_PATH) + 6);
		snprintf((char *)dev->dev_node_path, strlen(SERIAL_BASE_PATH) + 6,
				 "%scom%d", SERIAL_BASE_PATH, i + 1);

		dev->driver_data = &serial_ctxs[i];
		dev->ops = &serial_ops;

		serial_ctxs[i].base = base;
		serial_init_port(base);

		device_register(dev);
	}

	driver_register(&serial_driver);
	driver_bind_all();

	for (;;)
		;
}

void mod_exit(void)
{
}