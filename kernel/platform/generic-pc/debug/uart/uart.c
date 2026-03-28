/*********************************************************************************/
/* Module Name:  uart.c */
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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  */
/* SOFTWARE. */
/*********************************************************************************/

#include <arch/cpu/cpu.h>
#include <debug/uart.h>
#include <sys/spinlock.h>

#include <stddef.h>
#include <stdint.h>

typedef struct {
	const char *name;
	uint16_t base;
	uint8_t present;
} uart_port_t;

static uart_port_t uart_ports[] = {
#define UART_PORT_ENTRY(name, addr) { #name, (addr), 0 },
	UART_PORT_LIST(UART_PORT_ENTRY)
#undef UART_PORT_ENTRY
};

#define UART_PORT_COUNT (sizeof(uart_ports) / sizeof(uart_ports[0]))

static spinlock_t uart_lock;

static uint8_t uart_is_tx_empty(uint16_t base)
{
	return inb(base + 5) & 0x20;
}

static uint8_t uart_received(uint16_t base)
{
	return inb(base + 5) & 0x01;
}

static uint8_t uart_detect(uint16_t base)
{
	const uint8_t test_a = 0x55;
	const uint8_t test_b = 0xAA;

	outb(base + 7, test_a);
	if (inb(base + 7) != test_a)
		return 0;

	outb(base + 7, test_b);
	if (inb(base + 7) != test_b)
		return 0;

	return 1;
}

static void uart_init_one(uint16_t base)
{
	outb(base + 1, 0x00);
	outb(base + 3, 0x80);
	outb(base + 0, 0x03);
	outb(base + 1, 0x00);
	outb(base + 3, 0x03);
	outb(base + 2, 0xC7);
	outb(base + 4, 0x0B);
	outb(base + 4, 0x0F);
}

void serial_init(void)
{
	spinlock_init(&uart_lock);

	for (size_t i = 0; i < UART_PORT_COUNT; ++i) {
		uint16_t base = uart_ports[i].base;

		if (!uart_detect(base)) {
			uart_ports[i].present = 0;
			continue;
		}

		uart_init_one(base);
		uart_ports[i].present = 1;
	}
}

static void uart_send_one(uint16_t base, char c)
{
	while (uart_is_tx_empty(base) == 0)
		;
	outb(base, c);
}

void serial_sendbuf(const char *buf, size_t len)
{
	if (!buf || len == 0)
		return;

	spinlock_acquire(&uart_lock);
	for (size_t i = 0; i < UART_PORT_COUNT; ++i) {
		if (!uart_ports[i].present)
			continue;

		uint16_t base = uart_ports[i].base;
		for (size_t j = 0; j < len; j++)
			uart_send_one(base, buf[j]);
	}
	spinlock_release(&uart_lock);
}

void serial_send(char c)
{
	serial_sendbuf(&c, 1);
}

void serial_sendstr(char *s)
{
	if (!s)
		return;

	spinlock_acquire(&uart_lock);
	for (size_t i = 0; i < UART_PORT_COUNT; ++i) {
		if (!uart_ports[i].present)
			continue;

		uint16_t base = uart_ports[i].base;
		for (char *p = s; *p != '\0'; p++) {
			if (*p == '\r')
				continue;
			uart_send_one(base, *p);
		}
	}
	spinlock_release(&uart_lock);
}

void port_sendstr(uint16_t port, char *s)
{
	if (!s)
		return;

	spinlock_acquire(&uart_lock);
	while (*s != '\0') {
		if (*s == '\r') {
			s++;
			continue;
		}
		uart_send_one(port, *s++);
	}
	spinlock_release(&uart_lock);
}
