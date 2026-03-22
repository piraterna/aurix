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

#ifndef _USER_SYSCALL_H
#define _USER_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define MAX_SYSCALLS 1024

enum {
	SYS_EXIT = 0,
	SYS_OPEN = 1,
	SYS_READ = 2,
	SYS_WRITE = 3,
	SYS_CLOSE = 4,
	SYS_MOUNT = 5,
	SYS_IOCTL = 6,
	SYS_LOAD_MODULE = 7,
	SYS_EXEC = 8,
	SYS_MMAP = 9,
	SYS_LSEEK = 10,
	SYS_MUNMAP = 11,
	SYS_CLOCK_GET = 12,
	SYS_SET_FS_BASE = 13,
	SYS_MPROTECT = 14,
	SYS_GETCWD = 15,
	SYS_FORK = 16,
	SYS_CHDIR = 17,
	SYS_WAITPID = 18,
	SYS_EXECVE = 19,
};

typedef struct {
	uint64_t id;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rdx;
	uint64_t r10;
	uint64_t r8;
	uint64_t r9;
	uint64_t rip;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t rbx;
	uint64_t rbp;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
} syscall_args_t;

typedef int64_t (*syscall_handler_t)(const syscall_args_t *args);

typedef struct {
	syscall_handler_t handler;
	uint8_t valid;
	const char *name;
} syscall_entry_t;

extern syscall_entry_t syscall_table[MAX_SYSCALLS];

int register_syscall(uint32_t id, syscall_handler_t handler, const char *name);
int unregister_syscall(uint32_t id);
int64_t syscall_dispatch(uint32_t id, const syscall_args_t *args);

void syscall_builtin_init(void);

#endif // _USER_SYSCALL_H
