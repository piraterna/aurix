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

#include <boot/axprot.h>
#include <debug/log.h>
#include <dev/builtin/stdio.h>
#include <dev/device.h>
#include <dev/driver.h>
#include <flanterm/flanterm.h>
#include <lib/string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time/time.h>

struct stdio_termios;

int stdout_write(struct device *dev, const void *buf, size_t len,
				 size_t offset);
int stdin_read(struct device *dev, void *buf, size_t len, size_t offset);
int stdin_poll(struct device *dev);
int stdio_ioctl(struct device *dev, uint64_t cmd, void *arg);
int stdio_probe(struct device *dev);

static size_t stdio_collect_serial_devs(struct device **out, size_t max,
										bool need_read, bool need_write);
static struct device *stdio_find_dev(const char *path);
static int stdio_dev_has_data(struct device *dev);
static char stdio_scancode_to_ascii(uint8_t sc, bool shift, bool caps_lock);

static void stdin_rb_push(uint8_t ch);
static int stdin_rb_pop(uint8_t *out);
static int stdin_rb_count(void);
static void stdin_readable_flush(void);
static void stdin_line_flush(void);

static void stdin_handle_scancode(uint8_t sc);
static void stdin_handle_ascii(uint8_t ch);
static void stdio_process_input_byte(uint8_t ch);

static void stdio_echo_buf(const char *buf, size_t len);
static void stdio_echo_char(uint8_t ch);
static void stdio_echo_erase_one(void);

static void stdio_commit_line(void);
static void stdio_process_canonical(uint8_t ch);
static void stdio_process_noncanonical(uint8_t ch);

static void stdio_apply_termios(const struct stdio_termios *newt, uint64_t cmd);

#define STDIO_TCGETS 0x5401
#define STDIO_TCSETS 0x5402
#define STDIO_TCSETSW 0x5403
#define STDIO_TCSETSF 0x5404
#define STDIO_TIOCGWINSZ 0x5413
#define STDIO_TIOCSWINSZ 0x5414
#define STDIO_FIONREAD 0x541B

/*
 * Linux termios-ish bits we actually support.
 * Unsupported changes are masked off and warned about.
 */
#define STDIO_IGNBRK 0x00000001u
#define STDIO_BRKINT 0x00000002u
#define STDIO_IGNPAR 0x00000004u
#define STDIO_PARMRK 0x00000008u
#define STDIO_INPCK 0x00000010u
#define STDIO_ISTRIP 0x00000020u
#define STDIO_INLCR 0x00000040u
#define STDIO_IGNCR 0x00000080u
#define STDIO_ICRNL 0x00000100u
#define STDIO_IXON 0x00000400u
#define STDIO_IXOFF 0x00001000u
#define STDIO_IUTF8 0x00004000u

#define STDIO_OPOST 0x00000001u
#define STDIO_ONLCR 0x00000004u

#define STDIO_CSIZE 0x00000030u
#define STDIO_CS8 0x00000030u
#define STDIO_CREAD 0x00000080u
#define STDIO_CLOCAL 0x00000800u

#define STDIO_ISIG 0x00000001u
#define STDIO_ICANON 0x00000002u
#define STDIO_ECHO 0x00000008u
#define STDIO_ECHOE 0x00000010u
#define STDIO_ECHOK 0x00000020u
#define STDIO_ECHONL 0x00000040u
#define STDIO_NOFLSH 0x00000080u

#define STDIO_SUPPORTED_IFLAG (STDIO_INLCR | STDIO_IGNCR | STDIO_ICRNL)
#define STDIO_SUPPORTED_OFLAG (STDIO_OPOST | STDIO_ONLCR)
#define STDIO_SUPPORTED_CFLAG \
	(STDIO_CSIZE | STDIO_CS8 | STDIO_CREAD | STDIO_CLOCAL)
#define STDIO_SUPPORTED_LFLAG                                             \
	(STDIO_ISIG | STDIO_ICANON | STDIO_ECHO | STDIO_ECHOE | STDIO_ECHOK | \
	 STDIO_ECHONL)

#define STDIO_NCCS 32

enum {
	STDIN_VINTR = 0,
	STDIN_VQUIT = 1,
	STDIN_VERASE = 2,
	STDIN_VKILL = 3,
	STDIN_VEOF = 4,
	STDIN_VTIME = 5,
	STDIN_VMIN = 6,
	STDIN_VSTART = 8,
	STDIN_VSTOP = 9,
	STDIN_VSUSP = 10,
};

struct stdio_termios {
	uint32_t c_iflag;
	uint32_t c_oflag;
	uint32_t c_cflag;
	uint32_t c_lflag;
	uint8_t c_line;
	uint8_t c_cc[STDIO_NCCS];
	uint32_t c_ispeed;
	uint32_t c_ospeed;
};

struct stdio_winsize {
	uint16_t ws_row;
	uint16_t ws_col;
	uint16_t ws_xpixel;
	uint16_t ws_ypixel;
};

static struct stdio_winsize stdio_ws = {
	.ws_row = 25,
	.ws_col = 80,
	.ws_xpixel = 0,
	.ws_ypixel = 0,
};

static struct stdio_termios stdio_term = {
	.c_iflag = STDIO_ICRNL,
	.c_oflag = STDIO_OPOST | STDIO_ONLCR,
	.c_cflag = STDIO_CS8 | STDIO_CREAD | STDIO_CLOCAL,
	.c_lflag = STDIO_ISIG | STDIO_ICANON | STDIO_ECHO | STDIO_ECHOE | STDIO_ECHOK,
	.c_line = 0,
	.c_cc =
		{
			[STDIN_VINTR] = 3,	  /* ^C */
			[STDIN_VQUIT] = 28,	  /* ^\ */
			[STDIN_VERASE] = 127, /* DEL */
			[STDIN_VKILL] = 21,	  /* ^U */
			[STDIN_VEOF] = 4,	  /* ^D */
			[STDIN_VTIME] = 0,
			[STDIN_VMIN] = 1,
			[STDIN_VSTART] = 17,  /* ^Q */
			[STDIN_VSTOP] = 19,	  /* ^S */
			[STDIN_VSUSP] = 26,	  /* ^Z */
		},
	.c_ispeed = 0,
	.c_ospeed = 0,
};

struct device_ops stdout_ops = {
	.open = NULL,
	.close = NULL,
	.read = NULL,
	.write = stdout_write,
	.ioctl = stdio_ioctl,
	.poll = NULL,
};

struct device_ops stdin_ops = {
	.open = NULL,
	.close = NULL,
	.read = stdin_read,
	.write = NULL,
	.ioctl = stdio_ioctl,
	.poll = stdin_poll,
};

// TODO: proper stdout with ttys and shit
struct device stdout = {
	.name = "stdout",
	.class_name = "stdio",
	.dev_node_path = "stdout",
	.driver_data = NULL,
	.bound_driver = NULL,
	.ops = &stdout_ops,
	.next = NULL,
};

struct device stdin = {
	.name = "stdin",
	.class_name = "stdio",
	.dev_node_path = "stdin",
	.driver_data = NULL,
	.bound_driver = NULL,
	.ops = &stdin_ops,
	.next = NULL,
};

struct device stderr = {
	.name = "stderr",
	.class_name = "stdio",
	.dev_node_path = "stderr",
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

#define STDIO_SERIAL_PREFIX "/raw/serial/com"

#define STDIN_RB_SIZE 256u
#define STDIN_RB_MASK (STDIN_RB_SIZE - 1u)

#define STDIN_LINE_SIZE 1024u

static uint8_t stdin_rb[STDIN_RB_SIZE];
static uint32_t stdin_rb_head;
static uint32_t stdin_rb_tail;

static uint8_t stdin_linebuf[STDIN_LINE_SIZE];
static size_t stdin_line_len;
static bool stdin_line_eof_pending;

static bool stdin_left_shift;
static bool stdin_right_shift;
static bool stdin_caps_lock;
static bool stdin_extended;
static bool stdin_release_prefix;

int stdout_write(struct device *dev, const void *buf, size_t len, size_t offset)
{
	(void)dev;
	(void)offset;

	if (ft_ctx)
		flanterm_write(ft_ctx, (const char *)buf, len);

	struct device *serial_devs[MAX_DEVICES];
	size_t serial_count =
		stdio_collect_serial_devs(serial_devs, MAX_DEVICES, false, true);
	for (size_t i = 0; i < serial_count; i++) {
		struct device *serial = serial_devs[i];
		if (serial)
			serial->ops->write(serial, buf, len, 0);
	}

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
		if (strcmp(dev->dev_node_path, path) == 0)
			return dev;
	}

	return NULL;
}

static size_t stdio_collect_serial_devs(struct device **out, size_t max,
										bool need_read, bool need_write)
{
	if (!out || max == 0)
		return 0;

	size_t count = 0;
	for (int i = 0; i < device_count; i++) {
		struct device *dev = device_list[i];
		if (!dev || !dev->dev_node_path)
			continue;
		if (strncmp(dev->dev_node_path, STDIO_SERIAL_PREFIX,
					sizeof(STDIO_SERIAL_PREFIX) - 1) != 0) {
			continue;
		}
		if (!dev->ops)
			continue;
		if (need_read && !dev->ops->read)
			continue;
		if (need_write && !dev->ops->write)
			continue;
		if (count < max)
			out[count++] = dev;
	}

	return count;
}

static int stdio_dev_has_data(struct device *dev)
{
	if (!dev || !dev->ops || !dev->ops->read)
		return 0;
	if (!dev->ops->poll)
		return 1;
	return dev->ops->poll(dev) != 0;
}

static char stdio_scancode_to_ascii(uint8_t sc, bool shift, bool caps_lock)
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

	char ch = base[sc];
	if (!ch)
		return 0;

	if (ch >= 'a' && ch <= 'z') {
		bool upper = shift ^ caps_lock;
		return upper ? (char)(ch - 'a' + 'A') : ch;
	}

	return shift ? shifted[sc] : ch;
}

static void stdin_rb_push(uint8_t ch)
{
	uint32_t head = __atomic_load_n(&stdin_rb_head, __ATOMIC_RELAXED);
	uint32_t tail = __atomic_load_n(&stdin_rb_tail, __ATOMIC_ACQUIRE);

	if ((head - tail) >= STDIN_RB_SIZE) {
		__atomic_store_n(&stdin_rb_tail, tail + 1, __ATOMIC_RELEASE);
		tail++;
	}

	stdin_rb[head & STDIN_RB_MASK] = ch;
	__atomic_store_n(&stdin_rb_head, head + 1, __ATOMIC_RELEASE);
}

static int stdin_rb_pop(uint8_t *out)
{
	uint32_t tail = __atomic_load_n(&stdin_rb_tail, __ATOMIC_RELAXED);
	uint32_t head = __atomic_load_n(&stdin_rb_head, __ATOMIC_ACQUIRE);

	if (tail == head)
		return 0;

	*out = stdin_rb[tail & STDIN_RB_MASK];
	__atomic_store_n(&stdin_rb_tail, tail + 1, __ATOMIC_RELEASE);
	return 1;
}

static int stdin_rb_count(void)
{
	uint32_t head = __atomic_load_n(&stdin_rb_head, __ATOMIC_ACQUIRE);
	uint32_t tail = __atomic_load_n(&stdin_rb_tail, __ATOMIC_ACQUIRE);
	return (int)(head - tail);
}

static void stdin_readable_flush(void)
{
	__atomic_store_n(&stdin_rb_head, 0, __ATOMIC_RELEASE);
	__atomic_store_n(&stdin_rb_tail, 0, __ATOMIC_RELEASE);
}

static void stdin_line_flush(void)
{
	stdin_line_len = 0;
	stdin_line_eof_pending = false;
}

static void stdio_echo_buf(const char *buf, size_t len)
{
	if (!buf || len == 0)
		return;
	stdout_write(&stdout, buf, len, 0);
}

static void stdio_echo_char(uint8_t ch)
{
	if (!(stdio_term.c_lflag & STDIO_ECHO))
		return;

	if ((stdio_term.c_oflag & STDIO_OPOST) &&
		(stdio_term.c_oflag & STDIO_ONLCR) && ch == '\n') {
		static const char nl[] = "\r\n";
		stdio_echo_buf(nl, sizeof(nl) - 1);
		return;
	}

	stdio_echo_buf((const char *)&ch, 1);
}

static void stdio_echo_erase_one(void)
{
	if (!(stdio_term.c_lflag & STDIO_ECHO))
		return;
	if (!(stdio_term.c_lflag & STDIO_ECHOE))
		return;

	static const char bs[] = "\b \b";
	stdio_echo_buf(bs, sizeof(bs) - 1);
}

static void stdio_commit_line(void)
{
	for (size_t i = 0; i < stdin_line_len; i++)
		stdin_rb_push(stdin_linebuf[i]);
	stdin_line_len = 0;
}

static void stdio_process_canonical(uint8_t ch)
{
	uint8_t verase = stdio_term.c_cc[STDIN_VERASE];
	uint8_t vkill = stdio_term.c_cc[STDIN_VKILL];
	uint8_t veof = stdio_term.c_cc[STDIN_VEOF];
	uint8_t vintr = stdio_term.c_cc[STDIN_VINTR];
	uint8_t vquit = stdio_term.c_cc[STDIN_VQUIT];
	uint8_t vsusp = stdio_term.c_cc[STDIN_VSUSP];

	if ((stdio_term.c_lflag & STDIO_ISIG) &&
		(ch == vintr || ch == vquit || ch == vsusp)) {
		warn(
			"stdio: ISIG char 0x%x received but signal delivery is not implemented\n",
			(unsigned)ch);
		if (stdio_term.c_lflag & STDIO_ECHO)
			stdio_echo_char('^');
		if (stdio_term.c_lflag & STDIO_ECHO) {
			char c = '?';
			if (ch < 32)
				c = (char)('@' + ch);
			stdio_echo_char((uint8_t)c);
			stdio_echo_char('\n');
		}
		if (!(stdio_term.c_lflag & STDIO_NOFLSH)) {
			stdin_readable_flush();
			stdin_line_flush();
		}
		return;
	}

	if (ch == verase) {
		if (stdin_line_len > 0) {
			stdin_line_len--;
			stdio_echo_erase_one();
		}
		return;
	}

	if (ch == vkill) {
		while (stdin_line_len > 0) {
			stdin_line_len--;
			stdio_echo_erase_one();
		}
		if ((stdio_term.c_lflag & STDIO_ECHOK) &&
			(stdio_term.c_lflag & STDIO_ECHO))
			stdio_echo_char('\n');
		return;
	}

	if (ch == veof) {
		if (stdin_line_len > 0) {
			stdio_commit_line();
		} else {
			stdin_line_eof_pending = true;
		}
		return;
	}

	if (ch == '\n') {
		if (stdin_line_len < STDIN_LINE_SIZE)
			stdin_linebuf[stdin_line_len++] = ch;
		else
			warn("stdio: canonical input line buffer overflow\n");

		if ((stdio_term.c_lflag & STDIO_ECHO) ||
			(stdio_term.c_lflag & STDIO_ECHONL))
			stdio_echo_char('\n');

		stdio_commit_line();
		return;
	}

	if (stdin_line_len < STDIN_LINE_SIZE) {
		stdin_linebuf[stdin_line_len++] = ch;
		stdio_echo_char(ch);
	} else {
		warn("stdio: canonical input line buffer overflow\n");
	}
}

static void stdio_process_noncanonical(uint8_t ch)
{
	stdin_rb_push(ch);
	stdio_echo_char(ch);
}

static void stdio_process_input_byte(uint8_t ch)
{
	if (ch == '\r') {
		if (stdio_term.c_iflag & STDIO_IGNCR)
			return;
		if (stdio_term.c_iflag & STDIO_ICRNL)
			ch = '\n';
	} else if (ch == '\n') {
		if (stdio_term.c_iflag & STDIO_INLCR)
			ch = '\r';
	}

	if (stdio_term.c_lflag & STDIO_ICANON)
		stdio_process_canonical(ch);
	else
		stdio_process_noncanonical(ch);
}

static void stdin_handle_scancode(uint8_t sc)
{
	if (sc == 0xE0) {
		stdin_extended = true;
		return;
	}
	if (sc == 0xF0) {
		stdin_release_prefix = true;
		return;
	}

	if (stdin_extended) {
		stdin_extended = false;
		if (stdin_release_prefix) {
			stdin_release_prefix = false;
			return;
		}
		if (sc == 0x48 || sc == 0x75 || sc == 0x50 || sc == 0x72 ||
			sc == 0x4B || sc == 0x6B || sc == 0x4D || sc == 0x74) {
			stdio_process_input_byte('\033');
			stdio_process_input_byte('[');
			switch (sc) {
			case 0x48:
			case 0x75:
				stdio_process_input_byte('A');
				break;
			case 0x50:
			case 0x72:
				stdio_process_input_byte('B');
				break;
			case 0x4D:
			case 0x74:
				stdio_process_input_byte('C');
				break;
			case 0x4B:
			case 0x6B:
				stdio_process_input_byte('D');
				break;
			default:
				break;
			}
			return;
		}
		return;
	}

	if (stdin_release_prefix) {
		stdin_release_prefix = false;
		if (sc == 0x2A)
			stdin_left_shift = false;
		else if (sc == 0x36)
			stdin_right_shift = false;
		return;
	}

	if (sc == 0x3A || sc == 0x58) {
		stdin_caps_lock = !stdin_caps_lock;
		return;
	}

	if (sc == 0x2A || sc == 0x36) {
		if (sc == 0x2A)
			stdin_left_shift = true;
		else
			stdin_right_shift = true;
		return;
	}

	if (sc == 0xAA || sc == 0xB6) {
		if (sc == 0xAA)
			stdin_left_shift = false;
		else
			stdin_right_shift = false;
		return;
	}

	if (sc & 0x80) {
		uint8_t make = (uint8_t)(sc & 0x7F);
		if (make == 0x2A)
			stdin_left_shift = false;
		else if (make == 0x36)
			stdin_right_shift = false;
		return;
	}

	if (sc == 0x1C) {
		stdio_process_input_byte('\n');
		return;
	}

	if (sc == 0x0E) {
		stdio_process_input_byte(stdio_term.c_cc[STDIN_VERASE]);
		return;
	}

	bool shift = stdin_left_shift || stdin_right_shift;
	char ch = stdio_scancode_to_ascii(sc, shift, stdin_caps_lock);
	if (ch)
		stdio_process_input_byte((uint8_t)ch);
}

static void stdin_handle_ascii(uint8_t ch)
{
	stdio_process_input_byte(ch);
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
	struct device *serial_devs[MAX_DEVICES];
	size_t serial_count =
		stdio_collect_serial_devs(serial_devs, MAX_DEVICES, true, false);

	if (!kbd && serial_count == 0)
		return -1;

	char *out = buf;
	size_t n = 0;

	if (!(stdio_term.c_lflag & STDIO_ICANON)) {
		uint8_t vmin = stdio_term.c_cc[STDIN_VMIN];
		uint8_t vtime = stdio_term.c_cc[STDIN_VTIME];
		uint64_t idle_ms = 0;

		if (vmin == 0 && vtime == 0) {
			while (n < len) {
				uint8_t ch = 0;
				if (!stdin_rb_pop(&ch))
					break;
				out[n++] = (char)ch;
			}
			return (int)n;
		}

		while (1) {
			while (stdin_rb_count() > 0 && n < len) {
				uint8_t ch = 0;
				if (!stdin_rb_pop(&ch))
					break;
				out[n++] = (char)ch;
				idle_ms = 0;
				if (vmin > 0 && n >= vmin)
					return (int)n;
			}

			bool did_work = false;

			if (kbd && kbd->ops && kbd->ops->read && stdio_dev_has_data(kbd)) {
				uint8_t sc = 0;
				int got = kbd->ops->read(kbd, &sc, 1, 0);
				if (got > 0) {
					stdin_handle_scancode(sc);
					did_work = true;
				}
			}

			for (size_t i = 0; i < serial_count; i++) {
				struct device *serial = serial_devs[i];
				if (serial && stdio_dev_has_data(serial)) {
					uint8_t ch = 0;
					int got = serial->ops->read(serial, &ch, 1, 0);
					if (got > 0) {
						stdin_handle_ascii(ch);
						did_work = true;
					}
				}
			}

			if (stdin_rb_count() > 0)
				continue;

			if (!did_work) {
				sleep_ms(1);
				idle_ms++;
			}

			if (vmin == 0 && vtime > 0) {
				if (n > 0)
					return (int)n;
				if (idle_ms >= (uint64_t)vtime * 100u)
					return 0;
			}

			if (vmin > 0 && vtime > 0) {
				if (n > 0 && idle_ms >= (uint64_t)vtime * 100u)
					return (int)n;
			}
		}
	}

	while (1) {
		while (stdin_rb_count() == 0 && !stdin_line_eof_pending) {
			bool did_work = false;

			if (kbd && kbd->ops && kbd->ops->read && stdio_dev_has_data(kbd)) {
				uint8_t sc = 0;
				int got = kbd->ops->read(kbd, &sc, 1, 0);
				if (got > 0) {
					stdin_handle_scancode(sc);
					did_work = true;
				}
			}

			for (size_t i = 0; i < serial_count; i++) {
				struct device *serial = serial_devs[i];
				if (serial && stdio_dev_has_data(serial)) {
					uint8_t ch = 0;
					int got = serial->ops->read(serial, &ch, 1, 0);
					if (got > 0) {
						stdin_handle_ascii(ch);
						did_work = true;
					}
				}
			}

			if (!did_work)
				sleep_ms(1);
		}

		if (stdin_line_eof_pending && stdin_rb_count() == 0) {
			stdin_line_eof_pending = false;
			return 0;
		}

		while (n < len) {
			uint8_t ch = 0;
			if (!stdin_rb_pop(&ch))
				break;
			out[n++] = (char)ch;
			if (ch == '\n')
				return (int)n;
		}

		if (n > 0)
			return (int)n;
	}
}

int stdin_poll(struct device *dev)
{
	(void)dev;

	struct device *kbd = stdio_find_dev("/raw/ps2/kbd0");
	struct device *serial_devs[MAX_DEVICES];
	size_t serial_count =
		stdio_collect_serial_devs(serial_devs, MAX_DEVICES, true, false);

	if (stdin_rb_count() > 0 || stdin_line_eof_pending)
		return 1;

	if (stdio_dev_has_data(kbd))
		return 1;

	for (size_t i = 0; i < serial_count; i++) {
		if (stdio_dev_has_data(serial_devs[i]))
			return 1;
	}

	return 0;
}

static void stdio_apply_termios(const struct stdio_termios *newt, uint64_t cmd)
{
	struct stdio_termios oldt = stdio_term;
	struct stdio_termios applied = *newt;

	uint32_t bad_iflag = applied.c_iflag & ~STDIO_SUPPORTED_IFLAG;
	uint32_t bad_oflag = applied.c_oflag & ~STDIO_SUPPORTED_OFLAG;
	uint32_t bad_cflag = applied.c_cflag & ~STDIO_SUPPORTED_CFLAG;
	uint32_t bad_lflag = applied.c_lflag & ~STDIO_SUPPORTED_LFLAG;

	if (bad_iflag)
		warn("stdio: unsupported iflag bits 0x%x ignored\n", bad_iflag);
	if (bad_oflag)
		warn("stdio: unsupported oflag bits 0x%x ignored\n", bad_oflag);
	if (bad_cflag)
		warn("stdio: unsupported cflag bits 0x%x ignored\n", bad_cflag);
	if (bad_lflag)
		warn("stdio: unsupported lflag bits 0x%x ignored\n", bad_lflag);

	applied.c_iflag &= STDIO_SUPPORTED_IFLAG;
	applied.c_oflag &= STDIO_SUPPORTED_OFLAG;
	applied.c_cflag &= STDIO_SUPPORTED_CFLAG;
	applied.c_lflag &= STDIO_SUPPORTED_LFLAG;

	if (applied.c_line != 0) {
		warn(
			"stdio: line discipline switch requested (c_line=%u), only line discipline 0 is supported\n",
			(unsigned)applied.c_line);
		applied.c_line = 0;
	}

	if (applied.c_ispeed != 0 || applied.c_ospeed != 0) {
		warn(
			"stdio: termios speed switch requested (ispeed=%u ospeed=%u), speeds are ignored on stdio console\n",
			(unsigned)applied.c_ispeed, (unsigned)applied.c_ospeed);
		applied.c_ispeed = oldt.c_ispeed;
		applied.c_ospeed = oldt.c_ospeed;
	}

	if ((applied.c_iflag ^ oldt.c_iflag) != 0 ||
		(applied.c_oflag ^ oldt.c_oflag) != 0 ||
		(applied.c_cflag ^ oldt.c_cflag) != 0 ||
		(applied.c_lflag ^ oldt.c_lflag) != 0) {
		trace(
			"stdio: termios updated iflag=0x%x oflag=0x%x cflag=0x%x lflag=0x%x\n",
			applied.c_iflag, applied.c_oflag, applied.c_cflag, applied.c_lflag);
	}

	stdio_term = applied;

	if (cmd == STDIO_TCSETSF) {
		stdin_readable_flush();
		stdin_line_flush();
	} else if ((oldt.c_lflag & STDIO_ICANON) &&
			   !(stdio_term.c_lflag & STDIO_ICANON)) {
		/*
		 * Moving from canonical to noncanonical: pending edited input
		 * becomes immediately readable.
		 */
		for (size_t i = 0; i < stdin_line_len; i++)
			stdin_rb_push(stdin_linebuf[i]);
		stdin_line_len = 0;
	}
}

int stdio_ioctl(struct device *dev, uint64_t cmd, void *arg)
{
	(void)dev;

	switch (cmd) {
	case STDIO_TCGETS:
		if (!arg)
			return -1;
		*(struct stdio_termios *)arg = stdio_term;
		return 0;

	case STDIO_TCSETS:
	case STDIO_TCSETSW:
	case STDIO_TCSETSF:
		if (!arg)
			return -1;
		if (cmd == STDIO_TCSETSW)
			warn(
				"stdio: TCSETSW requested; output drain semantics are not implemented, applying immediately\n");
		stdio_apply_termios((const struct stdio_termios *)arg, cmd);
		return 0;

	case STDIO_TIOCGWINSZ:
		if (!arg)
			return -1;
		*(struct stdio_winsize *)arg = stdio_ws;
		return 0;

	case STDIO_TIOCSWINSZ:
		if (!arg)
			return -1;
		{
			struct stdio_winsize *ws = (struct stdio_winsize *)arg;
			if (ws->ws_row)
				stdio_ws.ws_row = ws->ws_row;
			if (ws->ws_col)
				stdio_ws.ws_col = ws->ws_col;
			stdio_ws.ws_xpixel = ws->ws_xpixel;
			stdio_ws.ws_ypixel = ws->ws_ypixel;
		}
		return 0;

	case STDIO_FIONREAD:
		if (!arg)
			return -1;
		*(int *)arg = stdin_rb_count() + (stdin_line_eof_pending ? 1 : 0);
		return 0;

	default:
		warn("stdio: unknown ioctl cmd 0x%lx\n", cmd);
		return -1;
	}
}

int stdio_probe(struct device *dev)
{
	if (strcmp(dev->class_name, "stdio") != 0)
		return -1;

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
