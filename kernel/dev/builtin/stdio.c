/*********************************************************************************/
/* Module Name:  stdio.c */
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

#include <dev/builtin/stdio.h>
#include <dev/driver.h>
#include <dev/device.h>
#include <util/kprintf.h>
#include <debug/log.h>
#include <lib/string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time/time.h>

int stdout_write(struct device *dev, const void *buf, size_t len,
				 size_t offset);
int stdin_read(struct device *dev, void *buf, size_t len, size_t offset);
int stdin_poll(struct device *dev);
int stdio_probe(struct device *dev);

struct device_ops stdout_ops = {
	.open = NULL,
	.close = NULL,
	.read = NULL,
	.write = stdout_write,
	.ioctl = NULL,
	.poll = NULL,
};

struct device_ops stdin_ops = {
	.open = NULL,
	.close = NULL,
	.read = stdin_read,
	.write = NULL,
	.ioctl = NULL,
	.poll = stdin_poll,
};

// TODO: proper stdout with ttys and shit
struct device stdout = {
	.name = "stdout",
	.class_name = "stdio",
	.dev_node_path = "/stdout",
	.driver_data = NULL,
	.bound_driver = NULL,
	.ops = &stdout_ops,
	.next = NULL,
};

struct device stdin = {
	.name = "stdin",
	.class_name = "stdio",
	.dev_node_path = "/stdin",
	.driver_data = NULL,
	.bound_driver = NULL,
	.ops = &stdin_ops,
	.next = NULL,
};

struct device stderr = {
	.name = "stderr",
	.class_name = "stdio",
	.dev_node_path = "/stderr",
	.driver_data = NULL,
	.bound_driver = NULL,
	.ops = &stdout_ops, // same as stdout for now
	.next = NULL,
};

struct driver stdio_driver = {
	.name = "stdio",
	.class_name = "stdio",
	.probe = stdio_probe,
	.remove = NULL,
};

int stdout_write(struct device *dev, const void *buf, size_t len, size_t offset)
{
	(void)dev;
	(void)offset;

	kprintf("%.*s", (int)len, (const char *)buf);

	return (int)len;
}

static struct device *stdio_find_dev(const char *path)
{
	if (!path)
		return NULL;

	for (int i = 0; i < device_count; i++) {
		struct device *dev = device_list[i];
		if (!dev || !dev->dev_node_path)
			continue;
		if (strcmp(dev->dev_node_path, path) == 0) {
			trace("stdio: found dev %s\n", dev->name);
			return dev;
		}
	}

	return NULL;
}

static int stdio_dev_has_data(struct device *dev)
{
	if (!dev || !dev->ops || !dev->ops->read)
		return 0;
	if (!dev->ops->poll)
		return 1;
	return dev->ops->poll(dev) != 0;
}

static char stdio_scancode_to_ascii(uint8_t sc, bool shift)
{
	static const char base[128] = {
		[0x02] = '1', [0x03] = '2',	 [0x04] = '3', [0x05] = '4', [0x06] = '5',
		[0x07] = '6', [0x08] = '7',	 [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
		[0x0C] = '-', [0x0D] = '=',	 [0x10] = 'q', [0x11] = 'w', [0x12] = 'e',
		[0x13] = 'r', [0x14] = 't',	 [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
		[0x18] = 'o', [0x19] = 'p',	 [0x1A] = '[', [0x1B] = ']', [0x1E] = 'a',
		[0x1F] = 's', [0x20] = 'd',	 [0x21] = 'f', [0x22] = 'g', [0x23] = 'h',
		[0x24] = 'j', [0x25] = 'k',	 [0x26] = 'l', [0x27] = ';', [0x28] = '\'',
		[0x29] = '`', [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',
		[0x2F] = 'v', [0x30] = 'b',	 [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
		[0x34] = '.', [0x35] = '/',	 [0x39] = ' ',
	};
	static const char shifted[128] = {
		[0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
		[0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
		[0x0C] = '_', [0x0D] = '+', [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E',
		[0x13] = 'R', [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
		[0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}', [0x1E] = 'A',
		[0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G', [0x23] = 'H',
		[0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = ':', [0x28] = '"',
		[0x29] = '~', [0x2B] = '|', [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C',
		[0x2F] = 'V', [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<',
		[0x34] = '>', [0x35] = '?', [0x39] = ' ',
	};
	if (sc >= 128)
		return 0;
	return shift ? shifted[sc] : base[sc];
}

#define STDIN_LINE_MAX 256

static char stdin_line[STDIN_LINE_MAX];
static size_t stdin_line_len;
static size_t stdin_line_pos;
static bool stdin_shift;
static bool stdin_extended;

static void stdin_echo_backspace(void)
{
	kprintf("\b \b");
}

static void stdin_push_char(char ch)
{
	if (stdin_line_len + 1 >= STDIN_LINE_MAX)
		return;
	stdin_line[stdin_line_len++] = ch;
	if (ch == '\n')
		kprintf("\n");
	else
		kprintf("%c", ch);
}

static void stdin_handle_backspace(void)
{
	if (stdin_line_len == 0)
		return;
	stdin_line_len--;
	stdin_echo_backspace();
}

static bool stdin_line_ready(void)
{
	return stdin_line_len > 0 && stdin_line[stdin_line_len - 1] == '\n';
}

static void stdin_reset_line(void)
{
	stdin_line_len = 0;
	stdin_line_pos = 0;
}

static void stdin_handle_scancode(uint8_t sc)
{
	if (sc == 0xE0) {
		stdin_extended = true;
		return;
	}

	if (stdin_extended) {
		stdin_extended = false;
		return;
	}

	if (sc == 0x2A || sc == 0x36) {
		stdin_shift = true;
		return;
	}
	if (sc == 0xAA || sc == 0xB6) {
		stdin_shift = false;
		return;
	}

	if (sc & 0x80)
		return;

	if (sc == 0x1C) {
		stdin_push_char('\n');
		return;
	}
	if (sc == 0x0E) {
		stdin_handle_backspace();
		return;
	}

	char ch = stdio_scancode_to_ascii(sc, stdin_shift);
	if (ch)
		stdin_push_char(ch);
}

static void stdin_handle_ascii(uint8_t ch)
{
	if (ch == '\r')
		ch = '\n';
	if (ch == '\b' || ch == 0x7F) {
		stdin_handle_backspace();
		return;
	}
	stdin_push_char((char)ch);
}

int stdin_read(struct device *dev, void *buf, size_t len, size_t offset)
{
	(void)dev;
	(void)offset;

	if (!buf)
		return -1;
	if (len == 0)
		return 0;

	struct device *kbd = stdio_find_dev("/raw/ps2/kbd0");
	struct device *com1 = stdio_find_dev("/raw/serial/com1");

	if (!kbd && !com1)
		return -1;

	char *out = buf;

	for (;;) {
		if (stdin_line_pos < stdin_line_len)
			break;

		stdin_reset_line();

		while (!stdin_line_ready()) {
			bool did_work = false;

			if (kbd && kbd->ops && kbd->ops->read && stdio_dev_has_data(kbd)) {
				uint8_t sc = 0;
				int got = kbd->ops->read(kbd, &sc, 1, 0);
				if (got > 0) {
					stdin_handle_scancode(sc);
					did_work = true;
				}
			}

			if (com1 && com1->ops && com1->ops->read &&
				stdio_dev_has_data(com1)) {
				uint8_t ch = 0;
				int got = com1->ops->read(com1, &ch, 1, 0);
				if (got > 0) {
					stdin_handle_ascii(ch);
					did_work = true;
				}
			}

			if (stdin_line_ready())
				break;

			if (!did_work)
				sleep_ms(1);
		}
	}

	size_t avail = stdin_line_len - stdin_line_pos;
	size_t to_copy = len < avail ? len : avail;
	memcpy(out, stdin_line + stdin_line_pos, to_copy);
	stdin_line_pos += to_copy;
	if (stdin_line_pos >= stdin_line_len)
		stdin_reset_line();
	return (int)to_copy;
}

int stdin_poll(struct device *dev)
{
	(void)dev;

	struct device *kbd = stdio_find_dev("/raw/ps2/kbd0");
	struct device *com1 = stdio_find_dev("/raw/serial/com1");

	if (stdio_dev_has_data(kbd))
		return 1;
	if (stdio_dev_has_data(com1))
		return 1;

	return 0;
}

int stdio_probe(struct device *dev)
{
	if (strcmp(dev->class_name, "stdio") != 0) {
		return -1;
	}

	if (strcmp(dev->name, "stdin") == 0)
		dev->ops = &stdin_ops;
	else
		dev->ops = &stdout_ops;
	dev->driver_data = NULL;
	trace("stdio: bound to device %s\n", dev->name ? dev->name : "(unnamed)");
	return 0;
}

void stdio_init()
{
	device_register(&stdin);
	device_register(&stdout);
	device_register(&stderr);
	driver_register(&stdio_driver);
	driver_bind_all();
	trace("stdio: initialized\n");
}
