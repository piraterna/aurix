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

#define KSH_HISTORY_SIZE 16
#define KSH_LINE_MAX 128

static char history[KSH_HISTORY_SIZE][KSH_LINE_MAX];
static int history_count = 0;
static int history_index = -1;

static void ksh_handle_ascii(char ch, char *line, size_t *len, size_t *cursor,
							 size_t cap)
{
	if (!line || !len || !cursor || cap == 0)
		return;

	if (ch == '\r' || ch == '\n') {
		kprintf("\n");
		line[*len] = 0;
		if (*len > 0) {
			int idx = history_count % KSH_HISTORY_SIZE;
			strcpy(history[idx], line);
			history_count++;
		}
		history_index = history_count;
		ksh_exec_line(line);
		*len = 0;
		*cursor = 0;
		line[0] = 0;
		kprintf("ksh> ");
		return;
	}

	if ((unsigned char)ch == 0x08 || (unsigned char)ch == 0x7F) {
		if (*cursor > 0) {
			for (size_t i = *cursor - 1; i < *len - 1; i++)
				line[i] = line[i + 1];
			line[*len - 1] = 0;
			(*len)--;
			(*cursor)--;
			kprintf("\b");
			for (size_t i = *cursor; i < *len; i++)
				kprintf("%c", line[i]);
			kprintf(" ");
			for (size_t i = *len; i >= *cursor; i--)
				kprintf("\b");
		}
		return;
	}

	if ((unsigned char)ch >= 0x20 && (unsigned char)ch <= 0x7E &&
		*len + 1 < cap) {
		for (size_t i = *len; i > *cursor; i--)
			line[i] = line[i - 1];
		line[*cursor] = ch;
		(*len)++;
		(*cursor)++;
		for (size_t i = *cursor - 1; i < *len; i++)
			kprintf("%c", line[i]);
		for (size_t i = *len; i > *cursor; i--)
			kprintf("\b");
	}
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
	if (!dev->ops->poll)
		return false;
	return dev->ops->poll(dev) != 0;
}

static char ksh_scancode_to_ascii(uint8_t sc, int shift)
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

void ksh_thread(void)
{
	char com1_path[] = "/dev/raw/serial/com1";
	struct fileio *com1 = open(com1_path, 0, 0);
	struct device *com1_dev = NULL;
	if (com1) {
		com1_dev = ksh_fileio_device(com1);
		if (!ksh_dev_can_read(com1_dev))
			kprintf("ksh: /dev/raw/serial/com1 read() not ready (skipping)\n");
	} else
		kprintf("ksh: /dev/raw/serial/com1 not available (skipping)\n");

	char kbd_path[] = "/dev/raw/ps2/kbd0";
	struct fileio *kbd = open(kbd_path, 0, 0);
	struct device *kbd_dev = NULL;
	if (kbd) {
		kbd_dev = ksh_fileio_device(kbd);
		if (!ksh_dev_can_read(kbd_dev))
			kprintf("ksh: /dev/raw/ps2/kbd0 read() not ready (skipping)\n");
	} else
		kprintf("ksh: /dev/raw/ps2/kbd0 not available (skipping)\n");

	if (!kbd && !com1) {
		kprintf("ksh: no input devices available\n");
		goto out;
	}

	kprintf("ksh: type 'help' for commands\n");
	kprintf("ksh> ");

	char line[KSH_LINE_MAX];
	size_t len = 0, cursor = 0;
	line[0] = 0;

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
				ksh_handle_ascii((char)b, line, &len, &cursor, sizeof(line));
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

		if (sc & 0x80)
			continue;

		switch (sc) {
		case 0x1C:
			ksh_handle_ascii('\n', line, &len, &cursor, sizeof(line));
			break;
		case 0x0E:
			ksh_handle_ascii(0x08, line, &len, &cursor, sizeof(line));
			break;
		case 0x4B:
			if (cursor > 0) {
				cursor--;
				kprintf("\b");
			}
			break;
		case 0x4D:
			if (cursor < len) {
				kprintf("%c", line[cursor]);
				cursor++;
			}
			break;
		case 0x48:
			if (history_count == 0)
				break;
			if (history_index > 0)
				history_index--;
			strcpy(line, history[history_index % KSH_HISTORY_SIZE]);
			len = strlen(line);
			cursor = len;
			kprintf("\r\x1b[K"
					"ksh> %s",
					line);
			break;
		case 0x50:
			if (history_index < history_count - 1)
				history_index++;
			else {
				history_index = history_count;
				line[0] = 0;
				len = cursor = 0;
			}
			strcpy(line, (history_index < history_count) ?
							 history[history_index % KSH_HISTORY_SIZE] :
							 "");
			len = strlen(line);
			cursor = len;
			kprintf("\r\x1b[K"
					"ksh> %s",
					line);
			break;
		default: {
			char ch = ksh_scancode_to_ascii(sc, 0);
			if (ch)
				ksh_handle_ascii(ch, line, &len, &cursor, sizeof(line));
			break;
		}
		}
	}

	if (kbd)
		close(kbd);
	if (com1)
		close(com1);

out:
	thread_exit(thread_current());
}