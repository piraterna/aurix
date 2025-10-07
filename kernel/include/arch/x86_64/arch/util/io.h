/*********************************************************************************/
/* Module Name:  io.h                                                            */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE     */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,   */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE   */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#ifndef _UTIL_IO_H
#define _UTIL_IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value)
{
	__asm__ volatile("outb %b0, %w1" : : "a"(value), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t value)
{
	__asm__ volatile("outw %w0, %w1" : : "a"(value), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t value)
{
	__asm__ volatile("outl %0, %w1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t value;
	__asm__ volatile("inb %w1, %b0" : "=a"(value) : "Nd"(port));
	return value;
}

static inline uint16_t inw(uint16_t port)
{
	uint16_t value;
	__asm__ volatile("inw %w1, %w0" : "=a"(value) : "Nd"(port));
	return value;
}

static inline uint32_t inl(uint16_t port)
{
	uint32_t value;
	__asm__ volatile("inl %w1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

static inline void io_wait(void)
{
	__asm__ volatile("outb %b0, %w1" : : "a"(0), "Nd"(0x80));
}

#endif /* _UTIL_IO_H */