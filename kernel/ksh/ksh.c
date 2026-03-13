/*********************************************************************************/
/* Module Name:  ksh.c */
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

#include <ksh/ksh.h>
#include <ksh/cmd.h>

#include <lib/string.h>
#include <sys/sched.h>
#include <time/time.h>
#include <util/kprintf.h>
#include <vfs/fileio.h>

#include <fs/devfs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static void ksh_handle_ascii(char ch, char *line, size_t *len, size_t cap)
{
	if (!line || !len || cap == 0)
		return;

	// Enter (CR or LF).
	if (ch == '\r' || ch == '\n') {
		kprintf("\n");
		line[*len] = 0;
		ksh_exec_line(line);
		*len = 0;
		line[0] = 0;
		kprintf("ksh> ");
		return;
	}

	// Backspace (BS or DEL).
	if ((unsigned char)ch == 0x08 || (unsigned char)ch == 0x7F) {
		if (*len > 0) {
			(*len)--;
			line[*len] = 0;
			kprintf("\b \b");
		}
		return;
	}

	// Keep it simple: accept printable ASCII only.
	if ((unsigned char)ch < 0x20 || (unsigned char)ch > 0x7E)
		return;

	if (*len + 1 >= cap)
		return;

	line[(*len)++] = ch;
	line[*len] = 0;
	kprintf("%c", ch);
}

static struct device *ksh_fileio_device(struct fileio *f)
{
	if (!f)
		return NULL;
	if (!(f->flags & SPECIAL_FILE_TYPE_DEVICE))
		return NULL;
	struct vnode *vn = (struct vnode *)f->private;
	if (!vn)
		return NULL;
	struct devfs_node *node = (struct devfs_node *)vn->node_data;
	if (!node)
		return NULL;
	return node->device;
}

static bool ksh_dev_can_read(struct device *dev)
{
	return dev && dev->ops && dev->ops->read;
}

static bool ksh_dev_has_data(struct device *dev)
{
	if (!dev || !dev->ops)
		return false;
	// Without poll(), we can't safely multiplex inputs (read() may block).
	if (!dev->ops->poll)
		return false;
	return dev->ops->poll(dev) != 0;
}

static char ksh_scancode_to_ascii(uint8_t sc, int shift)
{
	// US keyboard, PS/2 set 1 scancodes (i8042 translation enabled).
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

void ksh_thread(void)
{
	char com1_path[] = "/dev/raw/serial/com1";
	struct fileio *com1 = open(com1_path, 0);
	struct device *com1_dev = ksh_fileio_device(com1);
	if (com1 && !ksh_dev_can_read(com1_dev))
		kprintf("ksh: /dev/raw/serial/com1 read() not ready (skipping)\n");
	if (!com1)
		kprintf("ksh: /dev/raw/serial/com1 not available (skipping)\n");

	char kbd_path[] = "/dev/raw/ps2/kbd0";
	struct fileio *kbd = open(kbd_path, 0);
	struct device *kbd_dev = ksh_fileio_device(kbd);
	if (kbd && !ksh_dev_can_read(kbd_dev))
		kprintf("ksh: /dev/raw/ps2/kbd0 read() not ready (skipping)\n");
	if (!kbd)
		kprintf("ksh: /dev/raw/ps2/kbd0 not available (skipping)\n");

	if (!kbd && !com1) {
		kprintf("ksh: no input devices available\n");
		goto out;
	}

	kprintf("ksh: type 'help' for commands\n");
	kprintf("ksh> ");

	char line[128];
	size_t len = 0;
	line[0] = 0;

	bool lshift = false;
	bool rshift = false;
	bool extended = false;

	for (;;) {
		bool did_work = false;

		if (com1 && com1_dev && ksh_dev_has_data(com1_dev)) {
			for (unsigned i = 0; i < 32; i++) {
				if (!ksh_dev_has_data(com1_dev))
					break;
				uint8_t b = 0;
				size_t n = read(com1, 1, &b);
				if (n == (size_t)-1 || n == 0)
					break;
				did_work = true;
				ksh_handle_ascii((char)b, line, &len, sizeof(line));
			}
		}

		if (!kbd) {
			if (!did_work)
				sleep_ms(10);
			continue;
		}

		uint8_t sc = 0;
		if (kbd_dev && !ksh_dev_has_data(kbd_dev)) {
			if (!did_work)
				sleep_ms(10);
			continue;
		}
		size_t n = read(kbd, 1, &sc);
		if (n == (size_t)-1 || n == 0) {
			if (!did_work)
				sleep_ms(10);
			continue;
		}

		if (sc == 0xE0 || sc == 0xE1) {
			extended = true;
			continue;
		}

		if (extended) {
			// Ignore extended keys for this tiny shell.
			extended = false;
			continue;
		}

		// Shift press/release.
		if (sc == 0x2A) {
			lshift = true;
			continue;
		}
		if (sc == 0x36) {
			rshift = true;
			continue;
		}
		if (sc == 0xAA) {
			lshift = false;
			continue;
		}
		if (sc == 0xB6) {
			rshift = false;
			continue;
		}

		// Ignore key releases.
		if (sc & 0x80)
			continue;

		// Enter.
		if (sc == 0x1C) {
			ksh_handle_ascii('\n', line, &len, sizeof(line));
			continue;
		}

		// Backspace.
		if (sc == 0x0E) {
			ksh_handle_ascii(0x08, line, &len, sizeof(line));
			continue;
		}

		char ch = ksh_scancode_to_ascii(sc, (lshift || rshift) ? 1 : 0);
		if (!ch)
			continue;
		ksh_handle_ascii(ch, line, &len, sizeof(line));
	}

	// not reached
	if (kbd)
		close(kbd);
	if (com1)
		close(com1);

out:
	thread_exit(thread_current());
}
