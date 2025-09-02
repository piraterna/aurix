/*********************************************************************************/
/* Module Name:  uart.c */
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

#include <stdint.h>

#include <arch/cpu/cpu.h>
#include <uart/uart.h>

#define COM1 0x3f8

void uart_init(uint32_t baud_rate)
{
	uint8_t divisor = 115200 / baud_rate;

	outb(COM1 + 1, 0x00);
	outb(COM1 + 3, 0x80);
	outb(COM1 + 0, divisor & 0xFF);
	outb(COM1 + 1, (divisor >> 8) & 0xFF);
	outb(COM1 + 3, 0x03);
	outb(COM1 + 2, 0xC7);
	outb(COM1 + 4, 0x0B);
	outb(COM1 + 4, 0x0F);
}

void uart_send(char c)
{
	while ((inb(COM1 + 5) & 0x20) == 0)
		;
	outb(COM1, c);
}

void uart_sendstr(char *str)
{
	while (*str != '\0') {
		if (*str == '\n') {
			uart_send('\r');
		}
		uart_send(*str);
		str++;
	}
}