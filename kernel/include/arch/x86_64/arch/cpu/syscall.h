/*********************************************************************************/
/* Module Name:  syscall.h */
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

#ifndef _ARCH_CPU_SYSCALL_H
#define _ARCH_CPU_SYSCALL_H

#include <stdint.h>

typedef struct {
	uint64_t rax;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rdx;
	uint64_t r10;
	uint64_t r8;
	uint64_t r9;
	uint64_t rcx;
	uint64_t r11;
	uint64_t rsp;
} x86_64_syscall_frame_t;

void x86_64_syscall_init(void);
int64_t x86_64_syscall_dispatch(x86_64_syscall_frame_t *frame);

#endif /* _ARCH_CPU_SYSCALL_H */
