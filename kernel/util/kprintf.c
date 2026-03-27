/*********************************************************************************/
/* Module Name:  print.c */
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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
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
#include <debug/uart.h>

#include <lib/string.h>

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#include <boot/axprot.h>
#include <sys/spinlock.h>

#include <arch/cpu/cpu.h>

int32_t _fltused = 0;
int32_t __eqdf2 = 0;
int32_t __ltdf2 = 0;

static spinlock_t log_lock;

#define KLOG_BUFFER_SIZE (64u * 1024u)

static char klog_buf[KLOG_BUFFER_SIZE];
static size_t klog_head;
static size_t klog_size;

static void klog_append_locked(const char *buf, size_t len)
{
	if (!buf || len == 0)
		return;

	if (len >= KLOG_BUFFER_SIZE) {
		buf += len - KLOG_BUFFER_SIZE;
		len = KLOG_BUFFER_SIZE;
	}

	for (size_t i = 0; i < len; i++) {
		klog_buf[klog_head] = buf[i];
		klog_head = (klog_head + 1) % KLOG_BUFFER_SIZE;
		if (klog_size < KLOG_BUFFER_SIZE)
			klog_size++;
	}

	(void)buf;
	(void)len;
}

void log_init()
{
	spinlock_init(&log_lock);
}

size_t klog_get_size(void)
{
	spinlock_acquire(&log_lock);
	size_t size = klog_size;
	spinlock_release(&log_lock);
	return size;
}

size_t klog_read_at(void *out, size_t bytes, size_t offset)
{
	if (!out || bytes == 0)
		return 0;

	spinlock_acquire(&log_lock);
	size_t size = klog_size;
	if (offset >= size) {
		spinlock_release(&log_lock);
		return 0;
	}

	size_t to_copy = bytes;
	if (offset + to_copy > size)
		to_copy = size - offset;

	size_t start =
		(klog_head + KLOG_BUFFER_SIZE - size + offset) % KLOG_BUFFER_SIZE;
	size_t first = to_copy;
	if (start + first > KLOG_BUFFER_SIZE)
		first = KLOG_BUFFER_SIZE - start;

	memcpy(out, &klog_buf[start], first);
	if (to_copy > first)
		memcpy((uint8_t *)out + first, klog_buf, to_copy - first);

	spinlock_release(&log_lock);
	return to_copy;
}

int kprintf(const char *fmt, ...)
{
	uint8_t irq_state = save_if();
	cpu_disable_interrupts();

	spinlock_acquire(&log_lock);

	va_list args;
	va_start(args, fmt);
	char buffer[1024];
	int length = npf_vsnprintf(buffer, sizeof(buffer), fmt, args);

	if (length >= 0 && length < (int)sizeof(buffer)) {
		klog_append_locked(buffer, (size_t)length);
		serial_sendstr(buffer);
		if (ft_ctx)
			flanterm_write(ft_ctx, (char *)buffer, length);
	}

	va_end(args);

	spinlock_release(&log_lock);
	restore_if(irq_state);
	return length;
}

int klog_sink(const char *fmt, ...)
{
	uint8_t irq_state = save_if();
	cpu_disable_interrupts();

	spinlock_acquire(&log_lock);

	va_list args;
	va_start(args, fmt);
	char buffer[1024];
	int length = npf_vsnprintf(buffer, sizeof(buffer), fmt, args);

	if (length >= 0 && length < (int)sizeof(buffer)) {
		klog_append_locked(buffer, (size_t)length);
	}

	va_end(args);

	spinlock_release(&log_lock);
	restore_if(irq_state);
	return length;
}

int serial_kprintf(const char *fmt, ...)
{
	uint8_t irq_state = save_if();
	cpu_disable_interrupts();

	spinlock_acquire(&log_lock);

	va_list args;
	va_start(args, fmt);
	char buffer[1024];
	int length = npf_vsnprintf(buffer, sizeof(buffer), fmt, args);

	if (length >= 0 && length < (int)sizeof(buffer)) {
		serial_sendstr(buffer);
	}

	va_end(args);

	spinlock_release(&log_lock);
	restore_if(irq_state);
	return length;
}

int flanterm_kprintf(const char *fmt, ...)
{
	uint8_t irq_state = save_if();
	cpu_disable_interrupts();

	spinlock_acquire(&log_lock);

	va_list args;
	va_start(args, fmt);
	char buffer[1024];
	int length = npf_vsnprintf(buffer, sizeof(buffer), fmt, args);

	if (length >= 0 && length < (int)sizeof(buffer)) {
		if (ft_ctx)
			flanterm_write(ft_ctx, buffer, length);
	}

	va_end(args);

	spinlock_release(&log_lock);
	restore_if(irq_state);
	return length;
}

void _log_force_unlock()
{
	spinlock_release(&log_lock);
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = npf_vsnprintf(buf, size, fmt, args);
	va_end(args);
	return len;
}
