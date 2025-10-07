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

#include <arch/cpu/cpu.h>
#include <boot/aurix.h>
#include <debug/log.h>
#include <lib/align.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

pagetable *kernel_pm = NULL;

extern char _start_text[];
extern char _end_text[];
extern char _start_rodata[];
extern char _end_rodata[];
extern char _start_data[];
extern char _end_data[];

bool paging_init(void)
{
	if (kernel_pm) {
		warn("Kernel pagemap is already initialized!\n");
		return false;
	}

	kernel_pm = palloc(1);
	if (!kernel_pm) {
		error("Failed to allocate kernel pagemap!\n");
		return false;
	}

	memset(kernel_pm, 0, PAGE_SIZE);

	// kernel pagemap
	map_page(NULL, (uintptr_t)kernel_pm, (uintptr_t)kernel_pm,
			 VMM_PRESENT | VMM_WRITABLE);

	// mmap
	for (uint32_t i = 0; i < boot_params->mmap_entries; i++) {
		struct aurix_memmap *e = &boot_params->mmap[i];

		debug("%u: base=%p, size=%llu, type=%u\n", i, e->base, e->size,
			  e->type);

		if (e->type == AURIX_MMAP_RESERVED || e->type == AURIX_MMAP_KERNEL)
			continue;

		uint64_t flags = VMM_PRESENT;
		switch (e->type) {
		case AURIX_MMAP_USABLE:
		case AURIX_MMAP_ACPI_RECLAIMABLE:
		case AURIX_MMAP_BOOTLOADER_RECLAIMABLE:
		case AURIX_MMAP_FRAMEBUFFER:
			flags |= VMM_WRITABLE | VMM_NX;
			break;
		case AURIX_MMAP_ACPI_MAPPED_IO:
		case AURIX_MMAP_ACPI_MAPPED_IO_PORTSPACE:
		case AURIX_MMAP_ACPI_NVS:
			flags |= VMM_NX;
			break;
		default:
			break;
		}

		map_pages(NULL, e->base, e->base, e->size, flags);
		map_pages(NULL, e->base + boot_params->hhdm_offset, e->base, e->size,
				  flags);
	}

	// stack
	uint64_t stack = ALIGN_DOWN(boot_params->stack_addr, PAGE_SIZE);
	map_pages(NULL, stack, stack, 16 * 1024,
			  VMM_PRESENT | VMM_WRITABLE | VMM_NX);

	// kernel
	uint64_t text_end = ALIGN_UP((uint64_t)_end_text, PAGE_SIZE);
	map_pages(NULL, (uintptr_t)_start_text,
			  (uintptr_t)_start_text - 0xffffffff80000000 +
				  boot_params->kernel_addr,
			  text_end - (uintptr_t)_start_text, VMM_PRESENT);

	uint64_t rodata_end = ALIGN_UP((uint64_t)_end_rodata, PAGE_SIZE);
	map_pages(NULL, (uintptr_t)_start_rodata,
			  (uintptr_t)_start_rodata - 0xffffffff80000000 +
				  boot_params->kernel_addr,
			  rodata_end - (uintptr_t)_start_rodata, VMM_PRESENT | VMM_NX);

	uint64_t data_end = ALIGN_UP((uint64_t)_end_data, PAGE_SIZE);
	map_pages(
		NULL, (uintptr_t)_start_data,
		(uintptr_t)_start_data - 0xffffffff80000000 + boot_params->kernel_addr,
		data_end - (uintptr_t)_start_data, VMM_PRESENT | VMM_WRITABLE | VMM_NX);

	// make NULL cause a page fault
	unmap_page(NULL, (uintptr_t)NULL);

	boot_params = (struct aurix_parameters *)((uintptr_t)boot_params +
											  boot_params->hhdm_offset);
	write_cr3((uint64_t)kernel_pm);

	return true;
}

static inline void _map(pagetable *pm, uintptr_t virt, uintptr_t phys,
						uint64_t flags)
{
	uint64_t pml1_idx = (virt >> 12) & 0x1ff;
	uint64_t pml2_idx = (virt >> 21) & 0x1ff;
	uint64_t pml3_idx = (virt >> 30) & 0x1ff;
	uint64_t pml4_idx = (virt >> 39) & 0x1ff;

	// w^x
	if (flags & VMM_WRITABLE)
		flags |= VMM_NX;

	if (!(pm->entries[pml4_idx] & 1)) {
		void *pml4 = palloc(1);
		memset(pml4, 0, sizeof(pagetable));
		pm->entries[pml4_idx] = (uint64_t)pml4 | VMM_PRESENT | VMM_WRITABLE;
	}

	pagetable *pml3_table =
		(pagetable *)(pm->entries[pml4_idx] & 0x000FFFFFFFFFF000);
	if (!(pml3_table->entries[pml3_idx] & 1)) {
		void *pml3 = palloc(1);
		memset(pml3, 0, sizeof(pagetable));
		pml3_table->entries[pml3_idx] =
			(uint64_t)pml3 | VMM_PRESENT | VMM_WRITABLE;
	}

	pagetable *pml2_table =
		(pagetable *)(pml3_table->entries[pml3_idx] & 0x000FFFFFFFFFF000);
	if (!(pml2_table->entries[pml2_idx] & 1)) {
		void *pml2 = palloc(1);
		memset(pml2, 0, sizeof(pagetable));
		pml2_table->entries[pml2_idx] =
			(uint64_t)pml2 | VMM_PRESENT | VMM_WRITABLE;
	}

	pagetable *pml1_table =
		(pagetable *)(pml2_table->entries[pml2_idx] & 0x000FFFFFFFFFF000);
	pml1_table->entries[pml1_idx] = (phys & 0x000FFFFFFFFFF000) | flags;

	if (read_cr3() == (uint64_t)pm)
		invlpg((void *)virt);
}

static inline void _unmap(pagetable *pm, uintptr_t virt)
{
	uint64_t pml1_idx = (virt >> 12) & 0x1ff;
	uint64_t pml2_idx = (virt >> 21) & 0x1ff;
	uint64_t pml3_idx = (virt >> 30) & 0x1ff;
	uint64_t pml4_idx = (virt >> 39) & 0x1ff;

	if (!(pm->entries[pml4_idx] & 1)) {
		goto not_mapped;
	}

	pagetable *pml3_table =
		(pagetable *)(pm->entries[pml4_idx] & 0x000FFFFFFFFFF000);
	if (!(pml3_table->entries[pml3_idx] & 1)) {
		goto not_mapped;
	}

	pagetable *pml2_table =
		(pagetable *)(pml3_table->entries[pml3_idx] & 0x000FFFFFFFFFF000);
	if (!(pml2_table->entries[pml2_idx] & 1)) {
		goto not_mapped;
	}

	pagetable *pml1_table =
		(pagetable *)(pml2_table->entries[pml2_idx] & 0x000FFFFFFFFFF000);
	pml1_table->entries[pml1_idx] = 0;

	if (read_cr3() == (uint64_t)pm)
		invlpg((void *)virt);

	return;

not_mapped:
	warn("_unmap(): Page at address 0x%llx not mapped.\n", virt);
}

void map_pages(pagetable *pm, uintptr_t virt, uintptr_t phys, size_t size,
			   uint64_t flags)
{
	if (!pm)
		pm = kernel_pm;

	virt = ALIGN_DOWN(virt, PAGE_SIZE);
	phys = ALIGN_DOWN(phys, PAGE_SIZE);

	for (size_t i = 0; i < ALIGN_UP(size, PAGE_SIZE); i += PAGE_SIZE) {
		_map(pm, virt + i, phys + i, flags);
	}
}

void map_page(pagetable *pm, uintptr_t virt, uintptr_t phys, uint64_t flags)
{
	if (!pm)
		pm = kernel_pm;

	virt = ALIGN_DOWN(virt, PAGE_SIZE);
	phys = ALIGN_DOWN(phys, PAGE_SIZE);

	_map(pm, virt, phys, flags);
}

void unmap_pages(pagetable *pm, uintptr_t virt, size_t size)
{
	if (!pm)
		pm = kernel_pm;

	for (size_t i = 0; i < ALIGN_UP(size, PAGE_SIZE); i += PAGE_SIZE) {
		_unmap(pm, virt + i);
	}
}

void unmap_page(pagetable *pm, uintptr_t virt)
{
	if (!pm)
		pm = kernel_pm;

	_unmap(pm, virt);
}

pagetable *create_pagemap()
{
	pagetable *pm = (pagetable *)palloc(1);
	if (!pm) {
		error("create_pagemap(): Failed to allocate memory for a new pm.\n");
		return NULL;
	}
	pm = (pagetable *)ALIGN_UP((uint64_t)pm, PAGE_SIZE);
	memset(pm, 0, sizeof(pagetable));
	return pm;
}

uintptr_t virt_to_phys(pagetable *pm, uintptr_t virt)
{
	if (!pm)
		pm = kernel_pm;

	uint64_t pml4_idx = (virt >> 39) & 0x1ff;
	uint64_t pml3_idx = (virt >> 30) & 0x1ff;
	uint64_t pml2_idx = (virt >> 21) & 0x1ff;
	uint64_t pml1_idx = (virt >> 12) & 0x1ff;

	if (!(pm->entries[pml4_idx] & VMM_PRESENT)) {
		warn("virt_to_phys(): No mapping at PML4 for virt 0x%llx\n", virt);
		return 0;
	}

	pagetable *pml3_table =
		(pagetable *)(pm->entries[pml4_idx] & 0x000FFFFFFFFFF000);
	if (!(pml3_table->entries[pml3_idx] & VMM_PRESENT)) {
		warn("virt_to_phys(): No mapping at PML3 for virt 0x%llx\n", virt);
		return 0;
	}

	pagetable *pml2_table =
		(pagetable *)(pml3_table->entries[pml3_idx] & 0x000FFFFFFFFFF000);
	if (!(pml2_table->entries[pml2_idx] & VMM_PRESENT)) {
		warn("virt_to_phys(): No mapping at PML2 for virt 0x%llx\n", virt);
		return 0;
	}

	pagetable *pml1_table =
		(pagetable *)(pml2_table->entries[pml2_idx] & 0x000FFFFFFFFFF000);
	if (!(pml1_table->entries[pml1_idx] & VMM_PRESENT)) {
		warn("virt_to_phys(): No mapping at PML1 for virt 0x%llx\n", virt);
		return 0;
	}

	uintptr_t phys =
		(pml1_table->entries[pml1_idx] & 0x000FFFFFFFFFF000) | (virt & 0xFFF);
	return phys;
}