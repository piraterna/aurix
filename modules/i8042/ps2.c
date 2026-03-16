/*********************************************************************************/
/* Module Name:  ps2.c */
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

#include <sys/aurix/mod.h>

#include <dev/device.h>
#include <dev/driver.h>

#include <ps2.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int mod_init(void);
void mod_exit(void);

__attribute__((section(".aurix.mod"))) const struct axmod_info modinfo = {
	.name = "i8042_ps2",
	.desc = "i8042 PS/2 Controller",
	.author = "Jozef Nagy & Kevin Alavik",

	.mod_init = mod_init,
	.mod_exit = mod_exit,
};

// ===== i8042 controller helpers =====

static void ps2_flush_output(void)
{
	for (uint32_t spins = 0; spins < 1024; spins++) {
		uint8_t st = ax_inb(PS2_STATUS);
		if (!(st & PS2_STATUS_OUT_FULL))
			break;
		(void)ax_inb(PS2_DATA);
	}
}

bool ps2_wait_until_inbuf_free(void)
{
	uint64_t start_time = get_ms();
	uint64_t timeout_ms = 1000;

	while (ax_inb(PS2_STATUS) & PS2_STATUS_IN_FULL) {
		if (get_ms() - start_time > timeout_ms)
			return false;
	}

	return true;
}

static bool ps2_wait_until_outbuf_full(uint64_t timeout_ms)
{
	uint64_t start = get_ms();
	while (!(ax_inb(PS2_STATUS) & PS2_STATUS_OUT_FULL)) {
		if ((get_ms() - start) > timeout_ms)
			return false;
	}
	return true;
}

static void ps2_write_ctl_config(uint8_t val)
{
	ax_outb(PS2_COMMAND, 0x60);
	ax_outb(PS2_DATA, val);
}

static uint8_t ps2_read_ctl_config(void)
{
	ax_outb(PS2_COMMAND, 0x20);
	(void)ps2_wait_until_outbuf_full(100);
	return ax_inb(PS2_DATA);
}

bool ps2_send(uint8_t port, uint8_t val)
{
	if (port != PS2_PORT1 && port != PS2_PORT2)
		return false;

	if (port == PS2_PORT2)
		ax_outb(PS2_COMMAND, 0xD4);

	if (!ps2_wait_until_inbuf_free())
		return false;

	ax_outb(PS2_DATA, val);
	return true;
}

bool ps2_recv(uint8_t *out)
{
	if (!out)
		return false;
	if (!ps2_wait_until_outbuf_full(1000))
		return false;
	*out = ax_inb(PS2_DATA);
	return true;
}

static bool ps2_device_reset(uint8_t port)
{
	if (!ps2_send(port, 0xFF))
		return false;

	uint8_t b = 0;
	if (!ps2_recv(&b))
		return false;
	if (b != 0xFA)
		return false;

	if (!ps2_recv(&b))
		return false;
	return b == 0xAA;
}

void ps2_reset_port(uint8_t port)
{
	(void)ps2_device_reset(port);
}

void ps2_reset_all(void)
{
	ps2_reset_port(PS2_PORT1);
	ps2_reset_port(PS2_PORT2);
}

static bool ps2_test_port(uint8_t port)
{
	if (port == PS2_PORT1)
		ax_outb(PS2_COMMAND, 0xAB);
	else if (port == PS2_PORT2)
		ax_outb(PS2_COMMAND, 0xA9);
	else
		return false;

	uint8_t b = 0;
	if (!ps2_recv(&b))
		return false;
	return b == 0x00;
}

static void ps2_enable_port(uint8_t port)
{
	if (port == PS2_PORT1)
		ax_outb(PS2_COMMAND, 0xAE);
	else if (port == PS2_PORT2)
		ax_outb(PS2_COMMAND, 0xA8);
}

static void ps2_disable_port(uint8_t port)
{
	if (port == PS2_PORT1)
		ax_outb(PS2_COMMAND, 0xAD);
	else if (port == PS2_PORT2)
		ax_outb(PS2_COMMAND, 0xA7);
}

static bool ps2_set_scancode_set1(void)
{
	uint8_t resp;

	if (!ps2_send(PS2_PORT1, 0xF0))
		return false;
	if (!ps2_recv(&resp) || resp != 0xFA)
		return false;

	if (!ps2_send(PS2_PORT1, 0x01))
		return false;
	if (!ps2_recv(&resp) || resp != 0xFA)
		return false;

	return true;
}

static bool ps2_controller_init(void)
{
	ps2_disable_port(PS2_PORT1);
	ps2_disable_port(PS2_PORT2);
	ps2_flush_output();

	uint8_t cfg = ps2_read_ctl_config();
	cfg &= ~((1 << 0) | (1 << 1)); // disable IRQs
	cfg &= ~(1 << 6); // disable translation
	cfg &= ~((1 << 4) | (1 << 5)); // enable clocks
	ps2_write_ctl_config(cfg);

	// Controller self-test
	ax_outb(PS2_COMMAND, 0xAA);
	uint8_t st = 0;
	if (!ps2_recv(&st) || st != 0x55) {
		mod_log("controller self-test failed (0x%02x)\n", st);
		return false;
	}

	ps2_write_ctl_config(cfg);

	// Probe for second port
	bool dual = false;
	ax_outb(PS2_COMMAND, 0xA8);
	cfg = ps2_read_ctl_config();
	if ((cfg & (1u << 5)) == 0)
		dual = true;
	ax_outb(PS2_COMMAND, 0xA7);

	bool p1_ok = ps2_test_port(PS2_PORT1);
	bool p2_ok = dual ? ps2_test_port(PS2_PORT2) : false;

	if (!p1_ok && !p2_ok) {
		mod_log("no working ports\n");
		return false;
	}

	if (p1_ok) {
		ps2_enable_port(PS2_PORT1);
		ps2_set_scancode_set1(); // force set 1
	}
	if (p2_ok)
		ps2_enable_port(PS2_PORT2);

	mod_log("controller init ok (dual=%d)\n", dual ? 1 : 0);
	return true;
}

// ===== keyboard + mouse threads =====

#define PS2_RB_SIZE 256u
#define PS2_RB_MASK (PS2_RB_SIZE - 1u)

struct ps2_ring {
	uint8_t buf[PS2_RB_SIZE];
	uint32_t head;
	uint32_t tail;
};

static struct ps2_ring raw_kbd;
static struct ps2_ring raw_mouse;
static struct ps2_ring kbd_out;
static struct ps2_ring mouse_out;

static void rb_push(struct ps2_ring *rb, uint8_t b)
{
	uint32_t head = __atomic_load_n(&rb->head, __ATOMIC_RELAXED);
	uint32_t tail = __atomic_load_n(&rb->tail, __ATOMIC_ACQUIRE);
	if ((head - tail) >= PS2_RB_SIZE) {
		__atomic_store_n(&rb->tail, tail + 1, __ATOMIC_RELEASE);
		tail++;
	}
	rb->buf[head & PS2_RB_MASK] = b;
	__atomic_store_n(&rb->head, head + 1, __ATOMIC_RELEASE);
}

static int rb_pop(struct ps2_ring *rb, uint8_t *out)
{
	uint32_t tail = __atomic_load_n(&rb->tail, __ATOMIC_RELAXED);
	uint32_t head = __atomic_load_n(&rb->head, __ATOMIC_ACQUIRE);
	if (tail == head)
		return 0;
	*out = rb->buf[tail & PS2_RB_MASK];
	__atomic_store_n(&rb->tail, tail + 1, __ATOMIC_RELEASE);
	return 1;
}

static int rb_count(struct ps2_ring *rb)
{
	uint32_t head = __atomic_load_n(&rb->head, __ATOMIC_ACQUIRE);
	uint32_t tail = __atomic_load_n(&rb->tail, __ATOMIC_ACQUIRE);
	return (int)(head - tail);
}

static int ps2_poll_and_demux(void)
{
	int got = 0;
	for (;;) {
		uint8_t status = ax_inb(PS2_STATUS);
		if (!(status & PS2_STATUS_OUT_FULL))
			break;

		uint8_t data = ax_inb(PS2_DATA);
		if (status & PS2_STATUS_AUXDATA)
			rb_push(&raw_mouse, data);
		else
			rb_push(&raw_kbd, data);
		got++;
	}
	return got;
}

static bool ps2_is_dev_response(uint8_t b)
{
	return b == 0xFA || b == 0xAA || b == 0xFE;
}

static void ps2_kbd_thread(void)
{
	(void)ps2_send(PS2_PORT1, 0xF4);

	for (;;) {
		uint8_t sc = 0;
		if (!rb_pop(&raw_kbd, &sc)) {
			sleep_ms(1);
			continue;
		}
		if (!ps2_is_dev_response(sc))
			rb_push(&kbd_out, sc);
	}
}

static void ps2_mouse_thread(void)
{
	(void)ps2_send(PS2_PORT2, 0xF6);
	(void)ps2_send(PS2_PORT2, 0xF4);

	for (;;) {
		uint8_t b = 0;
		if (!rb_pop(&raw_mouse, &b)) {
			sleep_ms(1);
			continue;
		}
		if (b == 0xFA || b == 0xAA || b == 0xFE)
			continue;
		rb_push(&mouse_out, b);
	}
}

static void ps2_kbd_pump_inline(uint32_t budget)
{
	while (budget--) {
		uint8_t sc = 0;
		if (!rb_pop(&raw_kbd, &sc))
			break;
		if (!ps2_is_dev_response(sc))
			rb_push(&kbd_out, sc);
	}
}

static void ps2_mouse_pump_inline(uint32_t budget)
{
	while (budget--) {
		uint8_t b = 0;
		if (!rb_pop(&raw_mouse, &b))
			break;
		if (b == 0xFA || b == 0xAA || b == 0xFE)
			continue;
		rb_push(&mouse_out, b);
	}
}

static int kbd_open(struct device *dev)
{
	(void)dev;
	return 0;
}

static int kbd_close(struct device *dev)
{
	(void)dev;
	return 0;
}

static int kbd_read(struct device *dev, void *buf, size_t len, size_t offset)
{
	(void)dev;
	if (!buf)
		return -1;
	uint8_t *dst = buf;
	size_t n = 0;
	while (n < len) {
		uint8_t b;
		if (!rb_pop(&kbd_out, &b))
			break;
		dst[n++] = b;
	}
	return (int)n;
}

static int kbd_write(struct device *dev, const void *buf, size_t len,
					 size_t offset)
{
	(void)dev;
	(void)buf;
	(void)len;
	(void)offset;
	return -1;
}

static int kbd_ioctl(struct device *dev, uint64_t cmd, void *arg)
{
	(void)dev;
	(void)cmd;
	(void)arg;
	return 0;
}

static int kbd_poll(struct device *dev)
{
	(void)dev;
	return rb_count(&kbd_out) > 0;
}

static int mouse_open(struct device *dev)
{
	(void)dev;
	return 0;
}

static int mouse_close(struct device *dev)
{
	(void)dev;
	return 0;
}

static int mouse_read(struct device *dev, void *buf, size_t len, size_t offset)
{
	(void)dev;
	if (!buf)
		return -1;
	uint8_t *dst = buf;
	size_t n = 0;
	while (n < len) {
		uint8_t b;
		if (!rb_pop(&mouse_out, &b))
			break;
		dst[n++] = b;
	}
	return (int)n;
}

static int mouse_write(struct device *dev, const void *buf, size_t len,
					   size_t offset)
{
	(void)dev;
	(void)buf;
	(void)len;
	(void)offset;
	return -1;
}

static int mouse_ioctl(struct device *dev, uint64_t cmd, void *arg)
{
	(void)dev;
	(void)cmd;
	(void)arg;
	return 0;
}

static int mouse_poll(struct device *dev)
{
	(void)dev;
	return rb_count(&mouse_out) > 0;
}

static int kbd_open(struct device *dev);
static int kbd_close(struct device *dev);
static int kbd_read(struct device *dev, void *buf, size_t len, size_t offset);
static int kbd_write(struct device *dev, const void *buf, size_t len,
					 size_t offset);
static int kbd_ioctl(struct device *dev, uint64_t cmd, void *arg);
static int kbd_poll(struct device *dev);

static struct device_ops kbd_ops = {
	.open = kbd_open,
	.close = kbd_close,
	.read = kbd_read,
	.write = kbd_write,
	.ioctl = kbd_ioctl,
	.poll = kbd_poll,
};

static int mouse_open(struct device *dev);
static int mouse_close(struct device *dev);
static int mouse_read(struct device *dev, void *buf, size_t len, size_t offset);
static int mouse_write(struct device *dev, const void *buf, size_t len,
					   size_t offset);
static int mouse_ioctl(struct device *dev, uint64_t cmd, void *arg);
static int mouse_poll(struct device *dev);

static struct device_ops mouse_ops = {
	.open = mouse_open,
	.close = mouse_close,
	.read = mouse_read,
	.write = mouse_write,
	.ioctl = mouse_ioctl,
	.poll = mouse_poll,
};

static int ps2_probe(struct device *dev)
{
	if (!dev || !dev->name)
		return -1;

	if (streq(dev->name, "kbd0")) {
		dev->ops = &kbd_ops;
		dev->driver_data = NULL;
		mod_log("PS/2 keyboard probed: %s\n", dev->name);
		return 0;
	} else if (streq(dev->name, "mouse0")) {
		dev->ops = &mouse_ops;
		dev->driver_data = NULL;
		mod_log("PS/2 mouse probed: %s\n", dev->name);
		return 0;
	}

	mod_log("PS/2 probe: unknown device %s\n", dev->name);
	return -1;
}

static struct driver ps2_driver = {
	.name = "i8042_ps2",
	.class_name = "input",
	.probe = ps2_probe,
	.remove = NULL,
};

int mod_init(void)
{
	memset(&raw_kbd, 0, sizeof(raw_kbd));
	memset(&raw_mouse, 0, sizeof(raw_mouse));
	memset(&kbd_out, 0, sizeof(kbd_out));
	memset(&mouse_out, 0, sizeof(mouse_out));

	if (!ps2_controller_init()) {
		mod_log("PS/2 controller initialization failed\n");
		return -1;
	}
	ps2_flush_output();

	if (driver_register(&ps2_driver) != 0) {
		mod_log("failed to register PS/2 driver\n");
		return -1;
	}

	bool p1_ok = ps2_test_port(PS2_PORT1);
	bool p2_ok = ps2_test_port(PS2_PORT2);

	if (p1_ok) {
		struct device *kbd_dev = kmalloc(sizeof(*kbd_dev));
		if (kbd_dev) {
			memset(kbd_dev, 0, sizeof(*kbd_dev));
			kbd_dev->name = kmalloc(5);
			if (kbd_dev->name) {
				memcpy(kbd_dev->name, "kbd0", 5);
				kbd_dev->class_name = kmalloc(6);
				if (kbd_dev->class_name) {
					memcpy(kbd_dev->class_name, "input", 6);
					kbd_dev->dev_node_path = kmalloc(13);
					if (kbd_dev->dev_node_path) {
						memcpy(kbd_dev->dev_node_path, "/raw/ps2/kbd0", 13);
						kbd_dev->driver_data = NULL;
						kbd_dev->ops = NULL;

						if (device_register(kbd_dev) != 0) {
							mod_log("failed to register keyboard device\n");
							kfree(kbd_dev->dev_node_path);
							kfree(kbd_dev->class_name);
							kfree(kbd_dev->name);
							kfree(kbd_dev);
						} else {
							mod_log("registered PS/2 keyboard device\n");
						}
					} else {
						kfree(kbd_dev->class_name);
						kfree(kbd_dev->name);
						kfree(kbd_dev);
					}
				} else {
					kfree(kbd_dev->name);
					kfree(kbd_dev);
				}
			} else {
				kfree(kbd_dev);
			}
		}
	}

	if (p2_ok) {
		struct device *mouse_dev = kmalloc(sizeof(*mouse_dev));
		if (mouse_dev) {
			memset(mouse_dev, 0, sizeof(*mouse_dev));
			mouse_dev->name = kmalloc(6);
			if (mouse_dev->name) {
				memcpy(mouse_dev->name, "mouse0", 6);
				mouse_dev->class_name = kmalloc(6);
				if (mouse_dev->class_name) {
					memcpy(mouse_dev->class_name, "input", 6);
					mouse_dev->dev_node_path = kmalloc(14);
					if (mouse_dev->dev_node_path) {
						memcpy(mouse_dev->dev_node_path, "/raw/ps2/mouse0", 14);
						mouse_dev->driver_data = NULL;
						mouse_dev->ops = NULL;

						if (device_register(mouse_dev) != 0) {
							mod_log("failed to register mouse device\n");
							kfree((void *)mouse_dev->dev_node_path);
							kfree((void *)mouse_dev->class_name);
							kfree((void *)mouse_dev->name);
							kfree(mouse_dev);
						} else {
							mod_log("registered PS/2 mouse device\n");
						}
					} else {
						kfree((void *)mouse_dev->class_name);
						kfree((void *)mouse_dev->name);
						kfree(mouse_dev);
					}
				} else {
					kfree((void *)mouse_dev->name);
					kfree(mouse_dev);
				}
			} else {
				kfree(mouse_dev);
			}
		}
	}

	driver_bind_all();

	bool kbd_worker = false;
	bool mouse_worker = false;
	int ktid = -1;
	int mtid = -1;

	if (!make_child) {
		kprintf(
			"i8042_ps2: make_child AXAPI missing; running kbd/mouse inline\n");
	} else {
		ktid = make_child(ps2_kbd_thread);
		mtid = make_child(ps2_mouse_thread);
		kbd_worker = (ktid >= 0);
		mouse_worker = (mtid >= 0);
		mod_log("spawned kbd tid=%d mouse tid=%d\n", ktid, mtid);
		if (!kbd_worker)
			mod_log("failed to spawn kbd thread; inline fallback\n");
		if (!mouse_worker)
			kprintf(
				"i8042_ps2: failed to spawn mouse thread; inline fallback\n");
	}

	for (;;) {
		int got = ps2_poll_and_demux();
		if (!kbd_worker)
			ps2_kbd_pump_inline(64);
		if (!mouse_worker)
			ps2_mouse_pump_inline(64);
		if (got == 0)
			sleep_ms(1);
	}

	return 0;
}

void mod_exit(void)
{
}
