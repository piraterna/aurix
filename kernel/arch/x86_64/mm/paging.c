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

#include <boot/axprot.h>
#include <arch/cpu/cpu.h>
#include <lib/align.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <aurix.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define PAGE_FRAME_MASK 0x000FFFFFFFFFF000ULL
#define PML_IDX_MASK 0x1ffULL
#define PML_SHIFT_L1 12
#define PML_SHIFT_L2 21
#define PML_SHIFT_L3 30
#define PML_SHIFT_L4 39

pagetable *kernel_pm = NULL;

extern uint8_t *bitmap;
extern uint64_t bitmap_size;

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

	uintptr_t pm_phys = (uintptr_t)palloc(1);
	if (!pm_phys) {
		error("Failed to allocate kernel pagemap!\n");
		return false;
	}

	memset((void *)PHYS_TO_VIRT(pm_phys), 0, PAGE_SIZE);
	kernel_pm = (pagetable *)pm_phys;

	map_page(NULL, (uintptr_t)kernel_pm, (uintptr_t)kernel_pm,
			 VMM_PRESENT | VMM_WRITABLE);

	if (!boot_params->mmap || boot_params->mmap_entries == 0 ||
		boot_params->mmap_entries > 1000) {
		error("Invalid boot_params->mmap or mmap_entries: %p, %llu\n",
			  boot_params->mmap, boot_params->mmap_entries);
		return false;
	}

	uintptr_t mmap_phys = (uintptr_t)boot_params->mmap;
	map_pages(NULL, mmap_phys, mmap_phys,
			  ALIGN_UP(boot_params->mmap_entries * sizeof(struct aurix_memmap),
					   PAGE_SIZE),
			  VMM_PRESENT | VMM_WRITABLE | VMM_NX);

	for (uint32_t i = 0; i < boot_params->mmap_entries; i++) {
		struct aurix_memmap *e = &boot_params->mmap[i];
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

		map_pages(NULL, (uintptr_t)(PHYS_TO_VIRT(e->base)), (uintptr_t)e->base,
				  e->size, flags);
	}

	uint64_t stack = ALIGN_DOWN(boot_params->stack_addr, PAGE_SIZE);
	map_pages(NULL, stack, stack, 16 * 1024,
			  VMM_PRESENT | VMM_WRITABLE | VMM_NX);

	uintptr_t kvirt = 0xffffffff80000000ULL;
	uintptr_t kphys = boot_params->kernel_addr;

	debug("Mapping kernel at 0x%llx...\n", kvirt);
	uint64_t text_start = ALIGN_DOWN((uintptr_t)_start_text, PAGE_SIZE);
	uint64_t text_end = ALIGN_UP((uintptr_t)_end_text, PAGE_SIZE);
	map_pages(NULL, text_start, text_start - kvirt + kphys,
			  text_end - text_start, VMM_PRESENT);

	uint64_t rodata_start = ALIGN_DOWN((uintptr_t)_start_rodata, PAGE_SIZE);
	uint64_t rodata_end = ALIGN_UP((uintptr_t)_end_rodata, PAGE_SIZE);
	map_pages(NULL, rodata_start, rodata_start - kvirt + kphys,
			  rodata_end - rodata_start, VMM_PRESENT | VMM_NX);

	uint64_t data_start = ALIGN_DOWN((uintptr_t)_start_data, PAGE_SIZE);
	uint64_t data_end = ALIGN_UP((uintptr_t)_end_data, PAGE_SIZE);
	map_pages(NULL, data_start, data_start - kvirt + kphys,
			  data_end - data_start, VMM_PRESENT | VMM_WRITABLE | VMM_NX);

	// map bitmap
	debug("Mapping bitmap at %llx...\n", bitmap);
	map_pages(NULL, (uintptr_t)bitmap, VIRT_TO_PHYS(bitmap), bitmap_size,
			  VMM_PRESENT | VMM_WRITABLE | VMM_NX);

	unmap_page(NULL, (uintptr_t)NULL);

	boot_params->mmap = (struct aurix_memmap *)PHYS_TO_VIRT(boot_params->mmap);
	boot_params = (struct aurix_parameters *)PHYS_TO_VIRT(boot_params);

	write_cr3((uint64_t)kernel_pm);
	return true;
}

uint16_t pml1_index(uintptr_t v)
{
	return (v >> PML_SHIFT_L1) & PML_IDX_MASK;
}

uint16_t pml2_index(uintptr_t v)
{
	return (v >> PML_SHIFT_L2) & PML_IDX_MASK;
}

uint16_t pml3_index(uintptr_t v)
{
	return (v >> PML_SHIFT_L3) & PML_IDX_MASK;
}

uint16_t pml4_index(uintptr_t v)
{
	return (v >> PML_SHIFT_L4) & PML_IDX_MASK;
}

static uintptr_t alloc_pt_page_phys(void)
{
	uintptr_t p = (uintptr_t)palloc(1);
	if (!p)
		return 0;
	memset((void *)PHYS_TO_VIRT(p), 0, PAGE_SIZE);
	return p;
}

static inline void _map(pagetable *pm_phys_ptr, uintptr_t virt, uintptr_t phys,
						uint64_t flags)
{
	uintptr_t pm_phys =
		(pm_phys_ptr) ? (uintptr_t)pm_phys_ptr : (uintptr_t)kernel_pm;
	if (!pm_phys)
		return;

	virt = ALIGN_DOWN(virt, PAGE_SIZE);
	phys = ALIGN_DOWN(phys, PAGE_SIZE);

	uint64_t p4 = pml4_index(virt);
	uint64_t p3 = pml3_index(virt);
	uint64_t p2 = pml2_index(virt);
	uint64_t p1 = pml1_index(virt);

	if (flags & VMM_WRITABLE)
		flags |= VMM_NX;

	pagetable *pml4_table = (pagetable *)PHYS_TO_VIRT(pm_phys);

	if (!(pml4_table->entries[p4] & VMM_PRESENT)) {
		uintptr_t new_pml3_phys = alloc_pt_page_phys();
		if (!new_pml3_phys)
			return;
		pml4_table->entries[p4] =
			(new_pml3_phys & PAGE_FRAME_MASK) | VMM_PRESENT | VMM_WRITABLE;
	}

	pagetable *pml3_table =
		(pagetable *)PHYS_TO_VIRT(pml4_table->entries[p4] & PAGE_FRAME_MASK);

	if (!(pml3_table->entries[p3] & VMM_PRESENT)) {
		uintptr_t new_pml2_phys = alloc_pt_page_phys();
		if (!new_pml2_phys)
			return;
		pml3_table->entries[p3] =
			(new_pml2_phys & PAGE_FRAME_MASK) | VMM_PRESENT | VMM_WRITABLE;
	}

	pagetable *pml2_table =
		(pagetable *)PHYS_TO_VIRT(pml3_table->entries[p3] & PAGE_FRAME_MASK);

	if (!(pml2_table->entries[p2] & VMM_PRESENT)) {
		uintptr_t new_pml1_phys = alloc_pt_page_phys();
		if (!new_pml1_phys)
			return;
		pml2_table->entries[p2] =
			(new_pml1_phys & PAGE_FRAME_MASK) | VMM_PRESENT | VMM_WRITABLE;
	}

	pagetable *pml1_table =
		(pagetable *)PHYS_TO_VIRT(pml2_table->entries[p2] & PAGE_FRAME_MASK);

	pml1_table->entries[p1] =
		(phys & PAGE_FRAME_MASK) | (flags & ~PAGE_FRAME_MASK);

	if (read_cr3() == pm_phys)
		invlpg((void *)virt);
}

static inline void _unmap(pagetable *pm_phys_ptr, uintptr_t virt)
{
	uintptr_t pm_phys =
		(pm_phys_ptr) ? (uintptr_t)pm_phys_ptr : (uintptr_t)kernel_pm;
	if (!pm_phys)
		return;

	virt = ALIGN_DOWN(virt, PAGE_SIZE);

	uint64_t p4 = pml4_index(virt);
	uint64_t p3 = pml3_index(virt);
	uint64_t p2 = pml2_index(virt);
	uint64_t p1 = pml1_index(virt);

	pagetable *pml4_table = (pagetable *)PHYS_TO_VIRT(pm_phys);
	if (!(pml4_table->entries[p4] & VMM_PRESENT))
		goto not_mapped;

	pagetable *pml3_table =
		(pagetable *)PHYS_TO_VIRT(pml4_table->entries[p4] & PAGE_FRAME_MASK);
	if (!(pml3_table->entries[p3] & VMM_PRESENT))
		goto not_mapped;

	pagetable *pml2_table =
		(pagetable *)PHYS_TO_VIRT(pml3_table->entries[p3] & PAGE_FRAME_MASK);
	if (!(pml2_table->entries[p2] & VMM_PRESENT))
		goto not_mapped;

	pagetable *pml1_table =
		(pagetable *)PHYS_TO_VIRT(pml2_table->entries[p2] & PAGE_FRAME_MASK);
	pml1_table->entries[p1] = 0;

	if (read_cr3() == pm_phys)
		invlpg((void *)virt);
	return;

not_mapped:
	warn("_unmap(): Page at address 0x%llx not mapped.\n",
		 (unsigned long long)virt);
}

void map_pages(pagetable *pm, uintptr_t virt, uintptr_t phys, size_t size,
			   uint64_t flags)
{
	if (!pm)
		pm = (pagetable *)kernel_pm;
	virt = ALIGN_DOWN(virt, PAGE_SIZE);
	phys = ALIGN_DOWN(phys, PAGE_SIZE);
	size = ALIGN_UP(size, PAGE_SIZE);
	for (size_t off = 0; off < size; off += PAGE_SIZE)
		_map(pm, virt + off, phys + off, flags);
}

void map_page(pagetable *pm, uintptr_t virt, uintptr_t phys, uint64_t flags)
{
	if (!pm)
		pm = (pagetable *)kernel_pm;
	virt = ALIGN_DOWN(virt, PAGE_SIZE);
	phys = ALIGN_DOWN(phys, PAGE_SIZE);
	_map(pm, virt, phys, flags);
}

void unmap_pages(pagetable *pm, uintptr_t virt, size_t size)
{
	if (!pm)
		pm = (pagetable *)kernel_pm;
	virt = ALIGN_DOWN(virt, PAGE_SIZE);
	size = ALIGN_UP(size, PAGE_SIZE);
	for (size_t off = 0; off < size; off += PAGE_SIZE)
		_unmap(pm, virt + off);
}

void unmap_page(pagetable *pm, uintptr_t virt)
{
	if (!pm)
		pm = (pagetable *)kernel_pm;
	virt = ALIGN_DOWN(virt, PAGE_SIZE);
	_unmap(pm, virt);
}

pagetable *create_pagemap(void)
{
	uintptr_t pm_phys = (uintptr_t)palloc(1);
	if (!pm_phys) {
		error("create_pagemap(): Failed to allocate memory for a new pm.\n");
		return NULL;
	}
	memset((void *)PHYS_TO_VIRT(pm_phys), 0, PAGE_SIZE);
	return (pagetable *)pm_phys;
}
