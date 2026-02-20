/*********************************************************************************/
/* Module Name:  faulty.c */
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

#include <aurix/axapi.h>
#include <faulty.h>
#include <sys/aurix/mod.h>

__attribute__((section(".aurix.mod"))) const struct axmod_info modinfo = {
	.name = "Faulty Module",
	.desc = "Kernel panic/backtrace test module",
	.author = "AurixOS",

	.mod_init = mod_init,
	.mod_exit = mod_exit,
};

static __attribute__((noinline)) void panic_leaf(void)
{
	__asm__ volatile("int $0x01");
}

static __attribute__((noinline)) void panic_level2(unsigned x)
{
	volatile unsigned sink = x ^ 0x5a5a5a5aU;
	(void)sink;
	panic_leaf();
}

static __attribute__((noinline)) void panic_level1(unsigned x)
{
	volatile unsigned sink = x + 0x1337U;
	(void)sink;
	panic_level2(sink);
}

int mod_init(void)
{
	kprintf("faulty: triggering panic (cpu=%u)\n", cpu_get_current_id());
	panic_level1(0xdeadU);
	for (;;)
		sched_yield();
}

void mod_exit(void)
{
}
