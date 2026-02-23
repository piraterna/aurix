/*********************************************************************************/
/* Module Name:  serial.h */
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

#ifndef _SERIAL16550_H
#define _SERIAL16550_H

#include <stdint.h>

enum serial16550_status {
	SERIAL16550_NOT_PRESENT = 0,
	SERIAL16550_NOT_READY,
	SERIAL16550_RX_READY,
	SERIAL16550_TX_READY,
	SERIAL16550_READY,
	SERIAL16550_ERROR
};

int serial16550_poll(void);

int mod_init(void);
void mod_exit(void);

#endif /* _SERIAL16550_H */
