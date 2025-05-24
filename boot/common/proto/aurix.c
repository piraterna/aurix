/*********************************************************************************/
/* Module Name:  aurix.c                                                         */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#include <acpi/acpi.h>
#include <proto/aurix.h>
#include <loader/elf.h>
#include <mm/mman.h>
#include <mm/memmap.h>
#include <mm/vmm.h>
#include <lib/string.h>
#include <ui/framebuffer.h>
#include <vfs/vfs.h>
#include <print.h>
#include <axboot.h>

#include <stdint.h>
#include <stdbool.h>

#define AURIX_STACK_SIZE 16*1024

bool aurix_get_memmap(struct aurix_parameters *params, axboot_memmap *mmap, uint32_t mmap_entries, pagetable *pm)
{
	if (params == NULL || mmap == NULL || pm == NULL ||  mmap_entries == 0) {
		log("aurix_get_memmap(): Invalid parameter!\n");
		return false;
	}

	// UEFI returns an unnecessarily large memory map with regions of the same type
	// being split into multiple entries (probably due to memory attributes, which we do not care about).
	for (uint32_t i = 0; i < mmap_entries; i++) {
		if (i == mmap_entries - 1) {
			break;
		}
		if (mmap[i].base + mmap[i].size >= mmap[i+1].base && mmap[i].type == mmap[i+1].type) {
			mmap[i].size += mmap[i+1].size;
			for (uint32_t j = i+1; j < mmap_entries; j++) {
				mmap[j].base = mmap[j+1].base;
				mmap[j].size = mmap[j+1].size;
				mmap[j].type = mmap[j+1].type;
			}
			mmap_entries--;
			i--;
		}
	}

	// copy the memory map over to kernel parameters
	params->mmap = (struct aurix_memmap *)mem_alloc(sizeof(struct aurix_memmap) * mmap_entries);
	if (!(params->mmap)) {
		log("aurix_get_memmap(): Failed to allocate memory for storing memory map!\n");
		return false;
	}
	params->mmap_entries = mmap_entries;

	for (uint32_t i = 0; i < mmap_entries; i++) {
		params->mmap[i].base = mmap[i].base;
		params->mmap[i].size = mmap[i].size;
		switch (mmap[i].type) {
			case MemMapReserved:
			case MemMapFaulty:
			case MemMapFirmware:
				params->mmap[i].type = AURIX_MMAP_RESERVED;
				break;
			case MemMapACPIReclaimable:
				params->mmap[i].type = AURIX_MMAP_ACPI_RECLAIMABLE;
				break;
			case MemMapACPIMappedIO:
				params->mmap[i].type = AURIX_MMAP_ACPI_MAPPED_IO;
				break;
			case MemMapACPIMappedIOPortSpace:
				params->mmap[i].type = AURIX_MMAP_ACPI_MAPPED_IO_PORTSPACE;
				break;
			case MemMapACPINVS:
				params->mmap[i].type = AURIX_MMAP_ACPI_NVS;
				break;
			case MemMapFreeOnLoad:
				params->mmap[i].type = AURIX_MMAP_BOOTLOADER_RECLAIMABLE;
				break;
			case MemMapUsable:
				params->mmap[i].type = AURIX_MMAP_USABLE;
				break;
			default:
				log("aurix_get_memmap(): Unknown memory type in entry %u (%u), setting as reserved.\n", i, mmap[i].type);
				params->mmap[i].type = AURIX_MMAP_RESERVED;
				break;
		}
	}

	return true;
}

bool aurix_get_framebuffer(struct aurix_parameters *params)
{
	struct fb_mode *modes;
	int current_mode;
	uint32_t *fb_addr;

	params->framebuffer = (struct aurix_framebuffer *)mem_alloc(sizeof(struct aurix_framebuffer));
	if (!(params->framebuffer)) {
		log("aurix_get_framebuffer(): Failed to allocate memory for framebuffer information!\n");
		return false;
	}

	if (!get_framebuffer(&fb_addr, &modes, NULL, &current_mode)) {
		log("aurix_get_framebuffer(): get_framebuffer() returned false, setting everything to 0.\n");
		memset(params->framebuffer, 0, sizeof(struct aurix_framebuffer));
		return false;
	}

	params->framebuffer->addr = (uintptr_t)fb_addr;
	params->framebuffer->width = modes[current_mode].width;
	params->framebuffer->height = modes[current_mode].height;
	params->framebuffer->bpp = modes[current_mode].bpp;
	params->framebuffer->pitch = modes[current_mode].pitch;
	params->framebuffer->format = modes[current_mode].format;

	return true;
}

void aurix_load(char *kernel_path)
{
	char *kbuf = NULL;
	vfs_read(kernel_path, &kbuf);
	
	pagetable *pm = create_pagemap();
	if (!pm) {
		log("aurix_load(): Failed to create kernel pagemap! Halting...\n");
		// TODO: Halt
		while (1);
	}

	axboot_memmap *mmap;
	uint32_t mmap_entries = get_memmap(&mmap, pm);

	map_pages(pm, (uintptr_t)pm, (uintptr_t)pm, PAGE_SIZE, VMM_WRITABLE);

	void *stack = mem_alloc(AURIX_STACK_SIZE); // 16 KiB stack should be well more than enough
	if (!stack) {
		log("aurix_load(): Failed to allocate stack! Halting...\n");
		while (1);
	}
	memset(stack, 0, AURIX_STACK_SIZE);

	map_pages(pm, (uintptr_t)stack, (uintptr_t)stack, AURIX_STACK_SIZE, VMM_WRITABLE | VMM_NX);

	uintptr_t kernel_addr = 0;
	void *kernel_entry = (void *)elf_load(kbuf, &kernel_addr, pm);
	if (!kernel_entry) {
		log("aurix_load(): Failed to load '%s'! Halting...\n", kernel_path);
		while (1);
	}
	mem_free(kbuf);

	struct aurix_parameters parameters = {0};

	// set current revision
	parameters.revision = AURIX_PROTOCOL_REVISION;

	// translate memory map
	if (!aurix_get_memmap(&parameters, mmap, mmap_entries, pm)) {
		log("aurix_load(): Failed to aqcuire memory map!");
		while (1);
	}

	parameters.kernel_addr = kernel_addr;

	// get RSDP and SMBIOS
#ifdef ARCH_ACPI_AVAILABLE
	parameters.rsdp_addr = platform_get_rsdp();
#endif

#ifdef ARCH_SMBIOS_AVAILABLE
	parameters.smbios_addr = platform_get_smbios();
#endif

	// get framebuffer information
	if (!aurix_get_framebuffer(&parameters)) {
		log("aurix_load(): Failed to aqcuire framebuffer information!\n");
	}

	// map framebuffer
	map_pages(pm, parameters.framebuffer->addr, parameters.framebuffer->addr, parameters.framebuffer->height * parameters.framebuffer->pitch, VMM_WRITABLE);

	log("aurix_load(): Handoff state: pm=0x%llx, stack=0x%llx, kernel_entry=0x%llx\n", pm, stack, kernel_entry);
#ifdef AXBOOT_UEFI
	uefi_exit_bs();
#endif

	aurix_arch_handoff(kernel_entry, pm, stack, AURIX_STACK_SIZE, &parameters);
	__builtin_unreachable();
}
