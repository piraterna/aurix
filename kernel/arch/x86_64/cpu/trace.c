/*********************************************************************************/
/* Module Name:  trace.c */
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

#include <arch/cpu/cpu.h>
#include <cpu/trace.h>
#include <debug/log.h>
#include <stdint.h>
#include <mm/vmm.h>

void stack_trace(uint16_t max_depth)
{
	uintptr_t rbp;
	__asm__ volatile("movq %%rbp, %0" : "=r"(rbp)::"memory");

	uintptr_t prev_rbp = 0;
	for (uint16_t depth = 0; depth < max_depth; depth++) {
		if (rbp == 0)
			break;
		if (rbp & 0x7) /* must be 8-byte aligned */
			break;
		if (virt_to_phys(NULL, rbp) == 0 ||
			virt_to_phys(NULL, rbp + sizeof(void *)) == 0)
			break;
		uintptr_t *rbp_ptr = (uintptr_t *)rbp;
		uintptr_t saved_rip = rbp_ptr[1];
		trace("[%p] 0x%.16llx\n", (void *)rbp, (unsigned long long)saved_rip);
		if (saved_rip <= 0xffffffff80000000ULL)
			break;
		uintptr_t next_rbp = rbp_ptr[0];
		if (next_rbp == rbp || next_rbp == prev_rbp)
			break;
		prev_rbp = rbp;
		rbp = next_rbp;
	}
}
