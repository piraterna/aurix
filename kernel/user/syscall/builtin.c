/*********************************************************************************/
/* Module Name:  builtin.c */
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

#include <user/syscall.h>
#include <util/kprintf.h>
#include <sys/sched.h>
#include <stdarg.h>

#define SYS_EXIT 0
#define SYS_PRINT 1 // replace with stdio and fileio

int64_t sys_exit(void *args)
{
	syscall_args_t *sys_args = (syscall_args_t *)args;
	int64_t code = (int64_t)sys_args->rdi;
	info("TID=%u (%s) exited with code %lld\n", thread_current()->tid,
		 thread_current()->process->name ? thread_current()->process->name :
										   "unknown",
		 code);
	thread_exit(thread_current(), (int)code);
	return 0;
}

int64_t sys_print(void *args)
{
	syscall_args_t *sys_args = (syscall_args_t *)args;
	const char *str = (const char *)sys_args->rdi;
	size_t len = (size_t)sys_args->rsi;
	kprintf("%.*s", (int)len, str);
	return 0;
}

void syscall_builtin_init(void)
{
	register_syscall(SYS_EXIT, sys_exit);
	register_syscall(SYS_PRINT, sys_print);
}