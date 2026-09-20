/*********************************************************************************/
/* Module Name:  print.c                                                         */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE    */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,   */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE   */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#include <nanoprintf.h>

#include <util/kprintf.h>
#include <debug/log.h>
#include <debug/uart.h>
#include <lib/string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/spinlock.h>
#include <arch/cpu/cpu.h>
#include <platform/debug/uart.h>

#define KLOG_RING_SIZE (64u * 1024u)
#define MAX_CPUS 256
#define LOG_PERCPU_BUFFER_SIZE 4096
#define LOG_MAX_RECORDS_PER_CPU 32

static char klog_ring[KLOG_RING_SIZE];
static volatile size_t klog_ring_head = 0;
static volatile size_t klog_ring_size = 0;

static log_sink_t *g_sinks[LOG_SINK_MAX] = { 0 };
static spinlock_t g_sink_lock;

static struct log_percpu {
	char buffer[LOG_PERCPU_BUFFER_SIZE];
	size_t used;
} percpu_logs[MAX_CPUS];

static inline struct log_percpu *get_percpu_log(void)
{
	uint16_t cpu = cpu_get_current_id();
	if (cpu >= MAX_CPUS)
		cpu = 0;
	return &percpu_logs[cpu];
}

static void klog_ring_append(const char *buf, size_t len)
{
	if (!buf || len == 0)
		return;
	if (len >= KLOG_RING_SIZE) {
		buf += len - KLOG_RING_SIZE;
		len = KLOG_RING_SIZE;
	}
	size_t head = klog_ring_head;
	size_t space = KLOG_RING_SIZE - head;
	if (len <= space) {
		memcpy(&klog_ring[head], buf, len);
	} else {
		memcpy(&klog_ring[head], buf, space);
		memcpy(klog_ring, buf + space, len - space);
	}
	klog_ring_head = (head + len) % KLOG_RING_SIZE;
	size_t new_size = klog_ring_size + len;
	klog_ring_size = (new_size > KLOG_RING_SIZE) ? KLOG_RING_SIZE : new_size;
}

void log_init(void)
{
	spinlock_init(&g_sink_lock);
	for (int i = 0; i < MAX_CPUS; i++) {
		percpu_logs[i].used = 0;
	}
	log_early_init();
}

void log_sink_register(int id, log_sink_t *sink)
{
	if (id < 0 || id >= LOG_SINK_MAX)
		return;
	spinlock_acquire(&g_sink_lock);
	g_sinks[id] = sink;
	spinlock_release(&g_sink_lock);
}

void log_sink_unregister(int id)
{
	if (id < 0 || id >= LOG_SINK_MAX)
		return;
	spinlock_acquire(&g_sink_lock);
	g_sinks[id] = NULL;
	spinlock_release(&g_sink_lock);
}

void log_set_level(int sink_id, uint8_t level)
{
	if (sink_id < 0 || sink_id >= LOG_SINK_MAX)
		return;
	spinlock_acquire(&g_sink_lock);
	if (g_sinks[sink_id])
		g_sinks[sink_id]->min_level = level;
	spinlock_release(&g_sink_lock);
}

void log_set_flags(int sink_id, uint8_t flags)
{
	if (sink_id < 0 || sink_id >= LOG_SINK_MAX)
		return;
	spinlock_acquire(&g_sink_lock);
	if (g_sinks[sink_id])
		g_sinks[sink_id]->flags = flags;
	spinlock_release(&g_sink_lock);
}

static void log_write_sinks(uint8_t level, const char *func, const char *msg,
							size_t len)
{
	(void)len;
	uint64_t ts = get_ms();
	uint32_t secs = (uint32_t)(ts / 1000ull);
	uint32_t msecs = (uint32_t)(ts % 1000ull);

	bool msg_has_newline = (len > 0 && msg[len - 1] == '\n');

	for (int i = 0; i < LOG_SINK_MAX; i++) {
		log_sink_t *sink = g_sinks[i];
		if (!sink || !sink->write || !log_sink_enabled(sink, level))
			continue;

		char linebuf[LOG_PERCPU_BUFFER_SIZE];
		size_t pos = 0;

		if (sink->flags & LOG_SINK_FLAG_TIME) {
			pos += npf_snprintf(linebuf + pos, LOG_PERCPU_BUFFER_SIZE - pos,
								"[%u.%03u] ", secs, msecs);
		}

		if (sink->flags & LOG_SINK_FLAG_FUNC) {
			pos += npf_snprintf(linebuf + pos, LOG_PERCPU_BUFFER_SIZE - pos,
								"%s: ", func);
		}

		if (sink->flags & LOG_SINK_FLAG_LEVEL) {
			if (sink->flags & LOG_SINK_FLAG_COLOR) {
				const char *line_color = log_level_colors_line[level];
				const char *tag = log_level_tags[level];
				pos += npf_snprintf(linebuf + pos, LOG_PERCPU_BUFFER_SIZE - pos,
									"%s%s%s %s%s", LOG_STYLE_PREFIX, tag,
									LOG_STYLE_RESET, line_color, msg);
				pos += npf_snprintf(linebuf + pos, LOG_PERCPU_BUFFER_SIZE - pos,
									"%s", LOG_STYLE_RESET);
			} else {
				const char *tag = log_level_tags[level];
				pos += npf_snprintf(linebuf + pos, LOG_PERCPU_BUFFER_SIZE - pos,
									"%s: %s", tag, msg);
			}
		} else if (sink->flags & LOG_SINK_FLAG_COLOR) {
			const char *line_color = log_level_colors_line[level];
			pos += npf_snprintf(linebuf + pos, LOG_PERCPU_BUFFER_SIZE - pos,
								"%s%s%s", line_color, msg, LOG_STYLE_RESET);
		} else {
			pos += npf_snprintf(linebuf + pos, LOG_PERCPU_BUFFER_SIZE - pos,
								"%s", msg);
		}

		if (pos > 0 && pos < LOG_PERCPU_BUFFER_SIZE) {
			if (!msg_has_newline) {
				linebuf[pos++] = '\r';
				if (pos < LOG_PERCPU_BUFFER_SIZE)
					linebuf[pos++] = '\n';
			}
			sink->write(sink, linebuf, pos);
		}
	}
}

int log_write(uint8_t level, const char *func, const char *fmt, ...)
{
	if (level > LOG_MAX_LEVEL)
		return 0;

	va_list args;
	va_start(args, fmt);

	struct log_percpu *plog = get_percpu_log();
	int len = npf_vsnprintf(plog->buffer, LOG_PERCPU_BUFFER_SIZE, fmt, args);
	va_end(args);

	if (len <= 0 || (size_t)len >= LOG_PERCPU_BUFFER_SIZE)
		return len;

	klog_ring_append(plog->buffer, (size_t)len);
	log_write_sinks(level, func, plog->buffer, (size_t)len);

	return len;
}

int log_vwrite(uint8_t level, const char *func, const char *fmt, va_list args)
{
	if (level > LOG_MAX_LEVEL)
		return 0;

	struct log_percpu *plog = get_percpu_log();
	va_list args_copy;
	va_copy(args_copy, args);
	int len =
		npf_vsnprintf(plog->buffer, LOG_PERCPU_BUFFER_SIZE, fmt, args_copy);
	va_end(args_copy);

	if (len <= 0 || (size_t)len >= LOG_PERCPU_BUFFER_SIZE)
		return len;

	klog_ring_append(plog->buffer, (size_t)len);
	log_write_sinks(level, func, plog->buffer, (size_t)len);

	return len;
}

static void serial_sink_write(log_sink_t *sink, const char *buf, size_t len)
{
	(void)sink;
	serial_sendbuf(buf, len);
}

static void serial_sink_flush(log_sink_t *sink)
{
	(void)sink;
}

static void kcon_sink_write(log_sink_t *sink, const char *buf, size_t len)
{
	(void)sink;
#if CONFIG_KCONSOLE == 1
	extern void _e_kcon_puts(char str[], size_t len);
	_e_kcon_puts((char *)buf, len);
#endif
}

static void kcon_sink_flush(log_sink_t *sink)
{
	(void)sink;
}

static void klog_sink_write(log_sink_t *sink, const char *buf, size_t len)
{
	(void)sink;
	klog_ring_append(buf, len);
}

static void klog_sink_flush(log_sink_t *sink)
{
	(void)sink;
}

static log_sink_t g_serial_sink = {
	.write = serial_sink_write,
	.flush = serial_sink_flush,
	.min_level = LOG_LEVEL_ALL,
	.flags = LOG_SINK_FLAG_COLOR | LOG_SINK_FLAG_TIME | LOG_SINK_FLAG_FUNC |
			 LOG_SINK_FLAG_LEVEL,
	.priv = NULL,
};

static log_sink_t g_kcon_sink = {
	.write = kcon_sink_write,
	.flush = kcon_sink_flush,
	.min_level = LOG_LEVEL_ALL,
	.flags = LOG_SINK_FLAG_COLOR | LOG_SINK_FLAG_TIME | LOG_SINK_FLAG_FUNC,
	.priv = NULL,
};

static log_sink_t g_klog_sink = {
	.write = klog_sink_write,
	.flush = klog_sink_flush,
	.min_level = LOG_LEVEL_ALL,
	.flags = 0,
	.priv = NULL,
};

int kprintf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = log_vwrite(LOG_LEVEL_INFO, LOG_FUNC, fmt, args);
	va_end(args);
	return len;
}

int klog_sink(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = log_vwrite(LOG_LEVEL_INFO, LOG_FUNC, fmt, args);
	va_end(args);
	return len;
}

size_t klog_get_size(void)
{
	return klog_ring_size;
}

size_t klog_read_at(void *out, size_t bytes, size_t offset)
{
	if (!out || bytes == 0)
		return 0;
	if (offset >= klog_ring_size)
		return 0;

	size_t to_copy = bytes;
	if (offset + to_copy > klog_ring_size)
		to_copy = klog_ring_size - offset;

	size_t start = (klog_ring_head + KLOG_RING_SIZE - klog_ring_size + offset) %
				   KLOG_RING_SIZE;
	size_t first = to_copy;
	if (start + first > KLOG_RING_SIZE)
		first = KLOG_RING_SIZE - start;

	memcpy(out, &klog_ring[start], first);
	if (to_copy > first)
		memcpy((uint8_t *)out + first, klog_ring, to_copy - first);

	return to_copy;
}

int serial_kprintf(const char *fmt, ...)
{
	struct log_percpu *plog = get_percpu_log();
	va_list args;
	va_start(args, fmt);
	int len = npf_vsnprintf(plog->buffer, LOG_PERCPU_BUFFER_SIZE, fmt, args);
	va_end(args);

	if (len > 0 && (size_t)len < LOG_PERCPU_BUFFER_SIZE) {
		serial_sendbuf(plog->buffer, (size_t)len);
	}
	return len;
}

int kcon_kprintf(const char *fmt, ...)
{
#if CONFIG_KCONSOLE == 1
	extern void _e_kcon_puts(char str[], size_t len);
	struct log_percpu *plog = get_percpu_log();
	va_list args;
	va_start(args, fmt);
	int len = npf_vsnprintf(plog->buffer, LOG_PERCPU_BUFFER_SIZE, fmt, args);
	va_end(args);

	if (len > 0 && (size_t)len < LOG_PERCPU_BUFFER_SIZE) {
		_e_kcon_puts(plog->buffer, (size_t)len);
	}
	return len;
#else
	(void)fmt;
	return 0;
#endif
}

void _log_force_unlock(void)
{
	spinlock_release(&g_sink_lock);
}

void log_early_init(void)
{
	log_sink_register(LOG_SINK_SERIAL, &g_serial_sink);
	log_sink_register(2, &g_klog_sink);
#if CONFIG_KCONSOLE == 1
	log_sink_register(LOG_SINK_DISPLAY, &g_kcon_sink);
#endif
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = npf_vsnprintf(buf, size, fmt, args);
	va_end(args);
	return len;
}