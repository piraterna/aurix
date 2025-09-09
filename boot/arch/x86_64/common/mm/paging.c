/*********************************************************************************/
/* Module Name:  paging.c */
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

#include <axboot.h>
#include <lib/string.h>
#include <mm/mman.h>
#include <mm/vmm.h>
#include <print.h>
#include <stdint.h>

/* Trimmed down version of */
/* https://github.com/KevinAlavik/nekonix/blob/main/kernel/src/mm/vmm.c */
/* Thanks, Kevin <3 */

static void _map(pagetable *pm, uintptr_t virt, uintptr_t phys, uint64_t flags)
{
	uint64_t pml1_idx = (virt >> 12) & 0x1ff;
	uint64_t pml2_idx = (virt >> 21) & 0x1ff;
	uint64_t pml3_idx = (virt >> 30) & 0x1ff;
	uint64_t pml4_idx = (virt >> 39) & 0x1ff;

	// flags |= VMM_PRESENT;

	if (!(pm->entries[pml4_idx] & 1)) {
		void *pml4 = mem_alloc(PAGE_SIZE);
		memset(pml4, 0, sizeof(pagetable));
		pm->entries[pml4_idx] = (uint64_t)pml4 | VMM_PRESENT | VMM_WRITABLE;
	}

	pagetable *pml3_table =
		(pagetable *)(pm->entries[pml4_idx] & 0x000FFFFFFFFFF000);
	if (!(pml3_table->entries[pml3_idx] & 1)) {
		void *pml3 = mem_alloc(PAGE_SIZE);
		memset(pml3, 0, sizeof(pagetable));
		pml3_table->entries[pml3_idx] =
			(uint64_t)pml3 | VMM_PRESENT | VMM_WRITABLE;
	}

	pagetable *pml2_table =
		(pagetable *)(pml3_table->entries[pml3_idx] & 0x000FFFFFFFFFF000);
	if (!(pml2_table->entries[pml2_idx] & 1)) {
		void *pml2 = mem_alloc(PAGE_SIZE);
		memset(pml2, 0, sizeof(pagetable));
		pml2_table->entries[pml2_idx] =
			(uint64_t)pml2 | VMM_PRESENT | VMM_WRITABLE;
	}

	pagetable *pml1_table =
		(pagetable *)(pml2_table->entries[pml2_idx] & 0x000FFFFFFFFFF000);
	pml1_table->entries[pml1_idx] = (phys & 0x000FFFFFFFFFF000) | flags;
}

void map_pages(pagetable *pm, uintptr_t virt, uintptr_t phys, size_t size,
			   uint64_t flags)
{
	for (size_t i = 0; i < ROUND_UP(size, PAGE_SIZE); i += PAGE_SIZE) {
		_map(pm, virt + i, phys + i, flags);
	}

	debug("map_pages(): Mapped 0x%llx-0x%llx -> 0x%llx-0x%llx\n", phys,
		  phys + size, virt, virt + size);
}

void map_page(pagetable *pm, uintptr_t virt, uintptr_t phys, uint64_t flags)
{
	_map(pm, virt, phys, flags);
	debug("map_page(): Mapped 0x%llx -> 0x%llx\n", phys, virt);
}

pagetable *create_pagemap()
{
	pagetable *pm = (pagetable *)mem_alloc(PAGE_SIZE * 2);
	if (!pm) {
		debug("create_pagemap(): Failed to allocate memory for a new pm.\n");
		return NULL;
	}
	pm = (pagetable *)ROUND_UP((uint64_t)pm, PAGE_SIZE);
	memset(pm, 0, sizeof(pagetable));

	debug("create_pagemap(): Created new pm at 0x%llx\n", (uint64_t)pm);
	return pm;
}