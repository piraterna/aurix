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

#include <user/syscall.h>
#include <debug/log.h>
#include <sys/errno.h>
#include <arch/cpu/cpu.h>

syscall_entry_t syscall_table[MAX_SYSCALLS] = { 0 };

int register_syscall(uint32_t id, syscall_handler_t handler, const char *name)
{
	if (id >= MAX_SYSCALLS || !handler) {
		error("Failed to register syscall with invalid ID %u\n", id);
		return -1;
	}
	syscall_table[id].handler = handler;
	syscall_table[id].valid = 1;
	syscall_table[id].name = name;
	return 0;
}

int unregister_syscall(uint32_t id)
{
	if (id >= MAX_SYSCALLS || !syscall_table[id].valid) {
		error("Failed to unregister syscall with invalid ID %u\n", id);
		return -1;
	}
	syscall_table[id].handler = NULL;
	syscall_table[id].valid = 0;
	return 0;
}

int64_t syscall_dispatch(uint32_t id, const syscall_args_t *args)
{
	cpu_disable_interrupts();
	if (id >= MAX_SYSCALLS || !syscall_table[id].valid) {
		trace("Unknown syscall: %u\n", id);
		cpu_enable_interrupts();
		return -ENOSYS;
	}
	trace("syscall(%d) -> %s\n", id, syscall_table[id].name);
	int64_t r = syscall_table[id].handler(args);
	if (r > 0) {
		error("%s failed with: %s (%d)\n", syscall_table[id].name,
			  ERRNO_NAME(r), r);
	}
	cpu_enable_interrupts();
	return r;
}
