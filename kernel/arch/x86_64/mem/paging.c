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
#include <debug/print.h>
#include <lib/align.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

pagetable *kernel_pm = NULL;
extern char _kernel_start;
extern char _kernel_end;

bool paging_init(void)
{
	if (kernel_pm)
		return false;

	kernel_pm = palloc(1);
	if (!kernel_pm)
		return false;

	memset(kernel_pm, 0, PAGE_SIZE);

	for (uint32_t i = 0; i < boot_params->mmap_entries; i++) {
		struct aurix_memmap *e = &boot_params->mmap[i];

		if (e->type == AURIX_MMAP_RESERVED)
			continue;

		uint64_t flags = VMM_PRESENT;
		switch (e->type) {
			case AURIX_MMAP_USABLE:
			case AURIX_MMAP_ACPI_RECLAIMABLE:
			case AURIX_MMAP_BOOTLOADER_RECLAIMABLE:
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
		map_pages(NULL, e->base + boot_params->hhdm_offset, e->base, e->size, flags);
	}

	map_page(NULL, (uintptr_t)kernel_pm, (uintptr_t)kernel_pm, VMM_PRESENT | VMM_WRITABLE);

	klog("kernel_start: 0x%llx\nkernel_end: 0x%llx\n", &_kernel_start, &_kernel_end);

	map_pages(NULL, boot_params->kernel_addr, boot_params->kernel_addr, (uint64_t)&_kernel_end - (uint64_t)&_kernel_start, VMM_PRESENT | VMM_WRITABLE);
	map_pages(NULL, (uintptr_t)&_kernel_start, boot_params->kernel_addr, (uint64_t)&_kernel_end - (uint64_t)&_kernel_start, VMM_PRESENT | VMM_WRITABLE);
	write_cr3((uint64_t)kernel_pm);

	klog("Done!\n");
	return true;
}

static void inline _map(pagetable *pm, uintptr_t virt, uintptr_t phys, uint64_t flags)
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

static void inline _unmap(pagetable *pm, uintptr_t virt)
{
	uint64_t pml1_idx = (virt >> 12) & 0x1ff;
	uint64_t pml2_idx = (virt >> 21) & 0x1ff;
	uint64_t pml3_idx = (virt >> 30) & 0x1ff;
	uint64_t pml4_idx = (virt >> 39) & 0x1ff;

	if (!(pm->entries[pml4_idx] & 1)) {
		klog("_unmap(): Page at address 0x%llx not mapped.\n", virt);
		return;
	}

	pagetable *pml3_table =
		(pagetable *)(pm->entries[pml4_idx] & 0x000FFFFFFFFFF000);
	if (!(pml3_table->entries[pml3_idx] & 1)) {
		klog("_unmap(): Page at address 0x%llx not mapped.\n", virt);
		return;
	}

	pagetable *pml2_table =
		(pagetable *)(pml3_table->entries[pml3_idx] & 0x000FFFFFFFFFF000);
	if (!(pml2_table->entries[pml2_idx] & 1)) {
		klog("_unmap(): Page at address 0x%llx not mapped.\n", virt);
		return;
	}

	pagetable *pml1_table =
		(pagetable *)(pml2_table->entries[pml2_idx] & 0x000FFFFFFFFFF000);
	pml1_table->entries[pml1_idx] = 0;

	if (read_cr3() == (uint64_t)pm)
		invlpg((void *)virt);
}

void map_pages(pagetable *pm, uintptr_t virt, uintptr_t phys, size_t size,
			   uint64_t flags)
{
	if (!pm)
		pm = kernel_pm;

	for (size_t i = 0; i <= ALIGN_UP(size, PAGE_SIZE); i += PAGE_SIZE) {
		_map(pm, virt + i, phys + i, flags);
	}

	klog("map_pages(): Mapped 0x%llx-0x%llx -> 0x%llx-0x%llx\n", phys,
		  phys + (size * PAGE_SIZE), virt, virt + (size * PAGE_SIZE));
}

void map_page(pagetable *pm, uintptr_t virt, uintptr_t phys, uint64_t flags)
{
	if (!pm)
		pm = kernel_pm;

	_map(pm, virt, phys, flags);
	klog("map_page(): Mapped 0x%llx -> 0x%llx\n", phys, virt);
}

void unmap_pages(pagetable *pm, uintptr_t virt, size_t size)
{
	if (!pm)
		pm = kernel_pm;

	for (size_t i = 0; i < ALIGN_UP(size, PAGE_SIZE); i += PAGE_SIZE) {
		_unmap(pm, virt + i);
	}

	klog("map_pages(): Unmapped 0x%llx-0x%llx\n", virt, virt + (size * PAGE_SIZE));
}

void unmap_page(pagetable *pm, uintptr_t virt)
{
	if (!pm)
		pm = kernel_pm;

	_unmap(pm, virt);
	klog("map_page(): Unmapped 0x%llx\n", virt);
}

pagetable *create_pagemap()
{
	pagetable *pm = (pagetable *)palloc(PAGE_SIZE * 2);
	if (!pm) {
		klog("create_pagemap(): Failed to allocate memory for a new pm.\n");
		return NULL;
	}
	pm = (pagetable *)ALIGN_UP((uint64_t)pm, PAGE_SIZE);
	memset(pm, 0, sizeof(pagetable));

	klog("create_pagemap(): Created new pm at 0x%llx\n", (uint64_t)pm);
	return pm;
}
