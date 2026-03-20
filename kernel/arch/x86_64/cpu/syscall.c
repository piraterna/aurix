/*********************************************************************************/
/* Module Name:  syscall.c */
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

#include <arch/cpu/cpu.h>
#include <arch/cpu/syscall.h>
#include <sys/sched.h>
#include <sys/errno.h>
#include <user/syscall.h>

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084

#define EFER_SCE (1u << 0)

extern void x86_64_syscall_entry(void);

void x86_64_syscall_init(void)
{
	uint64_t efer = rdmsr(MSR_EFER);
	wrmsr(MSR_EFER, efer | EFER_SCE);

	uint64_t star = ((uint64_t)0x0013 << 48) | ((uint64_t)0x0008 << 32);
	wrmsr(MSR_STAR, star);
	wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)x86_64_syscall_entry);
	wrmsr(MSR_FMASK, (1u << 9));
}

int64_t x86_64_syscall_dispatch(x86_64_syscall_frame_t *frame)
{
	if (!frame)
		return -EFAULT;

	tcb *current = thread_current();
	if (!current || !current->process || !current->user)
		return -ENOSYS;

	syscall_args_t args = {
		.id = frame->rax,
		.rdi = frame->rdi,
		.rsi = frame->rsi,
		.rdx = frame->rdx,
		.r10 = frame->r10,
		.r8 = frame->r8,
		.r9 = frame->r9,
		.rip = frame->rcx,
		.rflags = frame->r11,
		.rsp = frame->rsp,
	};

	return syscall_dispatch((uint32_t)args.id, &args);
}
