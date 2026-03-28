/*********************************************************************************/
/* Module Name:  serial.h */
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

#ifndef _MACHINE_DEBUG_SERIAL_H
#define _MACHINE_DEBUG_SERIAL_H

#include <stdint.h>

#define UART_PORT_LIST(X) \
	X(COM1, 0x3F8)        \
	X(COM2, 0x2F8)        \
	X(COM3, 0x3E8)        \
	X(COM4, 0x2E8)        \
	X(COM5, 0x5F8)        \
	X(COM6, 0x4F8)        \
	X(COM7, 0x5E8)        \
	X(COM8, 0x4E8)

enum {
#define UART_ENUM_ENTRY(name, addr) name = (addr),
	UART_PORT_LIST(UART_ENUM_ENTRY)
#undef UART_ENUM_ENTRY
};

#endif /* _MACHINE_DEBUG_SERIAL_H */