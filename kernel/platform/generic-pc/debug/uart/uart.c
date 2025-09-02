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

#include <arch/cpu/cpu.h>
#include <debug/uart.h>

#include <stdint.h>

static uint8_t is_tx_empty(void)
{
	return inb(COM1 + 5) & 0x20;
}

static uint8_t received(void)
{
	return inb(COM1 + 5) & 1;
}

void serial_init(void)
{
	// TODO: Initialize all COM ports
	outb(COM1 + 1, 0x00);
	outb(COM1 + 3, 0x80);
	outb(COM1, 0x03);
	outb(COM1 + 1, 0x00);
	outb(COM1 + 3, 0x03);
	outb(COM1 + 2, 0xC7);
	outb(COM1 + 4, 0x0B);
	outb(COM1 + 4, 0x0F);
}

void serial_send(char c)
{
	while (is_tx_empty() == 0)
		;
	outb(COM1, c);
}

void serial_sendstr(char *s)
{
	while (*s != '\0') {
		if (*s == '\r') {
			s++;
			continue;
		}
		serial_send(*s++);
	}
}