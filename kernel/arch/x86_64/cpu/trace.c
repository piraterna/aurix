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
#include <loader/elf.h>
#include <mm/vmm.h>
#include <sys/ksyms.h>
#include <sys/sched.h>
#include <aurix.h>
#include <stdint.h>

#define KERNEL_BASE 0xffffffff80000000ULL

bool trace_lookup_symbol(uintptr_t addr, const char **name_out,
						 uintptr_t *sym_addr_out)
{
	if (!name_out)
		return false;
	*name_out = NULL;
	if (sym_addr_out)
		*sym_addr_out = 0;

	if (addr == 0)
		return false;

	if (addr >= KERNEL_BASE)
		return ksym_lookup(addr, name_out, sym_addr_out);

	tcb *t = thread_current();
	pcb *p = t ? t->process : NULL;
	if (!p || !p->image_elf)
		return false;

	return elf_lookup_addr(p->image_elf, addr, name_out, sym_addr_out);
}

void stack_trace(uint16_t max_depth)
{
	uintptr_t rbp;
	__asm__ volatile("movq %%rbp, %0" : "=r"(rbp)::"memory");
	stack_trace_from(0, rbp, max_depth);
}

void stack_trace_from(uintptr_t pm_phys, uintptr_t rbp, uint16_t max_depth)
{
	pagetable *pm = pm_phys ? (pagetable *)pm_phys : NULL;
	uintptr_t prev_rbp = 0;
	for (uint16_t depth = 0; depth < max_depth; depth++) {
		if (rbp == 0)
			break;
		if (rbp & 0x7) /* must be 8-byte aligned */
			break;
		if (vget_phys(pm, rbp) == 0 || vget_phys(pm, rbp + sizeof(void *)) == 0)
			break;
		uintptr_t *rbp_ptr = (uintptr_t *)rbp;
		uintptr_t saved_rip = rbp_ptr[1];
		if (saved_rip == 0)
			break;

		const char *name = NULL;
		uintptr_t sym = 0;
		if (trace_lookup_symbol(saved_rip, &name, &sym) && name) {
			trace("[%p] 0x%.16llx <%s+0x%llx>\n", (void *)rbp,
				  (unsigned long long)saved_rip, name,
				  (unsigned long long)(saved_rip - sym));
		} else {
			trace("[%p] 0x%.16llx\n", (void *)rbp,
				  (unsigned long long)saved_rip);
		}
		uintptr_t next_rbp = rbp_ptr[0];
		if (next_rbp == rbp || next_rbp == prev_rbp)
			break;
		prev_rbp = rbp;
		rbp = next_rbp;
	}
}
