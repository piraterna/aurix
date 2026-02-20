/*********************************************************************************/
/* Module Name:  mod.h */
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

#ifndef __SYS_AURIX_MOD_H
#define __SYS_AURIX_MOD_H

#include <stdint.h>

#define AXMOD_EXPORTS_VADDR 0x500000

struct axmod_exports {
	int (*kprintf)(const char *fmt, ...);
	void (*sched_yield)(void);
} __attribute__((packed));

struct axmod_info {
	char *name;
	char *desc;
	char *author;

	int (*mod_init)(void);
	void (*mod_exit)(void);
} __attribute__((packed));

#endif /* __SYS_AURIX_MOD_H */
