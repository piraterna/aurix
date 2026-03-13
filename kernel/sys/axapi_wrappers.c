/*********************************************************************************/
/* Module Name:  axapi_wrappers.c */
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

#include <aurix/axapi.h>
#include <arch/cpu/cpu.h>
#include <stdint.h>
#include <sys/sched.h>

int make_child_ex(void (*entry)(void), uint64_t flags)
{
	(void)flags;
	if (!entry)
		return -1;
	tcb *cur = thread_current();
	if (!cur || !cur->process)
		return -1;

	tcb *t = thread_create(cur->process, entry);
	if (!t)
		return -1;
	return (int)t->tid;
}

int make_child(void (*entry)(void))
{
	return make_child_ex(entry, 0);
}

uint8_t ax_inb(uint16_t port)
{
	return inb(port);
}

void ax_outb(uint16_t port, uint8_t val)
{
	outb(port, val);
}

void ax_io_wait(void)
{
	io_wait();
}
