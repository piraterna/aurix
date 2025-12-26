/*********************************************************************************/
/* Module Name:  aurix.c */
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

#include <acpi/acpi.h>
#include <aurix_logo.h>
#include <axboot.h>
#include <lib/string.h>
#include <loader/elf.h>
#include <mm/memmap.h>
#include <mm/mman.h>
#include <mm/vmm.h>
#include <print.h>
#include <proto/aurix.h>
#include <ui/framebuffer.h>
#include <vfs/vfs.h>

#include <stdbool.h>
#include <stdint.h>

#define AURIX_STACK_SIZE 16 * 1024

void aurix_load(char *kernel_path);

void aurix_arch_handoff(void *kernel_entry, pagetable *pm, void *stack,
						uint32_t stack_size,
						struct aurix_parameters *parameters);

inline void mmap_insert_blank(struct aurix_memmap **mmap,
							  uint32_t *mmap_entries, uint32_t *index)
{
	*mmap_entries += 1;
	for (uint32_t j = *mmap_entries; j > *index; j--) {
		(*mmap)[j].base = (*mmap)[j - 1].base;
		(*mmap)[j].size = (*mmap)[j - 1].size;
		(*mmap)[j].type = (*mmap)[j - 1].type;
	}
}

// this function expects addr and size to be page-aligned
inline void mmap_add_entry(struct aurix_memmap **mmap, uintptr_t addr,
						   size_t size, int type, uint32_t *index,
						   uint32_t *mmap_entries, bool split)
{
	uint64_t entry_base = (*mmap)[*index].base;
	uint64_t entry_size = (*mmap)[*index].size;

	*index += 1;
	mmap_insert_blank(mmap, mmap_entries, index);

	(*mmap)[*index].base = addr;
	(*mmap)[*index].size = size;
	(*mmap)[*index].type = type;

	if (split) {
		(*mmap)[*index - 1].size = addr - entry_base;
		*index += 1;

		if (entry_base + entry_size > addr + size) {
			mmap_insert_blank(mmap, mmap_entries, index);

			(*mmap)[*index].base = addr + size;
			(*mmap)[*index].size = entry_size - (addr - entry_base) - size;
			(*mmap)[*index].type = AURIX_MMAP_USABLE;
		}
	}
}

bool aurix_get_memmap(struct aurix_parameters *params, size_t kernel_size,
					  axboot_memmap *mmap, uint32_t mmap_entries, pagetable *pm)
{
	uintptr_t kernel_addr = params->kernel_addr;

	if (params == NULL || mmap == NULL || pm == NULL || mmap_entries == 0) {
		error("aurix_get_memmap(): Invalid parameter!\n");
		return false;
	}

	// UEFI returns an unnecessarily large memory map with regions of the same
	// type being split into multiple entries (probably due to memory
	// attributes, which we do not care about).
	for (uint32_t i = 0; i < mmap_entries; i++) {
		if (i == mmap_entries - 1) {
			break;
		}
		if (mmap[i].base + mmap[i].size >= mmap[i + 1].base &&
			mmap[i].type == mmap[i + 1].type) {
			mmap[i].size += mmap[i + 1].size;
			for (uint32_t j = i + 1; j < mmap_entries; j++) {
				mmap[j].base = mmap[j + 1].base;
				mmap[j].size = mmap[j + 1].size;
				mmap[j].type = mmap[j + 1].type;
			}
			mmap_entries--;
			i--;
		}
	}

	// copy the memory map over to kernel parameters
	params->mmap = (struct aurix_memmap *)mem_alloc(
		sizeof(struct aurix_memmap) * (mmap_entries + 5));
	if (!(params->mmap)) {
		error(
			"aurix_get_memmap(): Failed to allocate memory for storing memory map!\n");
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
		case MemMapUsable:
			params->mmap[i].type = AURIX_MMAP_USABLE;
			break;
		default:
			debug(
				"aurix_get_memmap(): Unknown memory type in entry %u (%u), setting as reserved.\n",
				i, mmap[i].type);
			params->mmap[i].type = AURIX_MMAP_RESERVED;
			break;
		}
	}

	kernel_size =
		ROUND_UP(kernel_addr - ROUND_DOWN(kernel_addr, PAGE_SIZE) + kernel_size,
				 PAGE_SIZE) +
		PAGE_SIZE;
	kernel_addr = ROUND_DOWN(kernel_addr, PAGE_SIZE);

	size_t params_size = ROUND_UP(sizeof(struct aurix_parameters), PAGE_SIZE);
	uintptr_t params_addr = ROUND_DOWN((uintptr_t)params, PAGE_SIZE);

	size_t mmap_size =
		ROUND_UP((uintptr_t)params->mmap -
					 ROUND_DOWN((uintptr_t)params->mmap, PAGE_SIZE) +
					 sizeof(struct aurix_memmap) * params->mmap_entries,
				 PAGE_SIZE) +
		PAGE_SIZE;
	uintptr_t mmap_addr = ROUND_DOWN((uintptr_t)params->mmap, PAGE_SIZE);

	size_t framebuffer_size = ROUND_UP(
		params->framebuffer.height * params->framebuffer.pitch, PAGE_SIZE);
	uintptr_t framebuffer_addr =
		ROUND_DOWN(params->framebuffer.addr - params->hhdm_offset, PAGE_SIZE);

	size_t cmdline_size = ROUND_UP(sizeof(params->cmdline), PAGE_SIZE);
	uintptr_t cmdline_addr =
		(params->cmdline == NULL) ?
			0 :
			ROUND_DOWN((uintptr_t)params->cmdline - params->hhdm_offset,
					   PAGE_SIZE);

	for (uint32_t i = 0; i < mmap_entries; i++) {
		uintptr_t base = params->mmap[i].base;
		size_t size = params->mmap[i].size;

		// create a kernel entry!
		if (base <= kernel_addr && base + size >= kernel_addr) {
			mmap_add_entry(&params->mmap, kernel_addr, kernel_size,
						   AURIX_MMAP_KERNEL, &i, &params->mmap_entries, true);
		}

		// and the boot parameters!
		if (base <= params_addr && base + size >= params_addr) {
			mmap_add_entry(&params->mmap, params_addr, params_size,
						   AURIX_MMAP_BOOTLOADER_RECLAIMABLE, &i,
						   &params->mmap_entries, true);
		}
		if (base <= mmap_addr && base + size >= mmap_addr) {
			mmap_add_entry(&params->mmap, mmap_addr, mmap_size,
						   AURIX_MMAP_BOOTLOADER_RECLAIMABLE, &i,
						   &params->mmap_entries, true);
		}
		if (cmdline_addr != 0 && base <= cmdline_addr &&
			base + size >= cmdline_addr) {
			mmap_add_entry(&params->mmap, cmdline_addr, cmdline_size,
						   AURIX_MMAP_BOOTLOADER_RECLAIMABLE, &i,
						   &params->mmap_entries, true);
		}

		// ...and the framebuffer
		if (base + size <= framebuffer_addr &&
			(i == mmap_entries - 1 ||
			 (i != mmap_entries - 1 &&
			  params->mmap[i + 1].base >=
				  framebuffer_addr + framebuffer_size))) {
			mmap_add_entry(&params->mmap, framebuffer_addr, framebuffer_size,
						   AURIX_MMAP_FRAMEBUFFER, &i, &params->mmap_entries,
						   false);
		}
	}

	return true;
}

#ifdef AXBOOT_UEFI
#include <efi.h>
#include <efilib.h>

EFI_GUID bootargs_guid = { 0xd8637320,
						   0x2230,
						   0x4748,
						   { 0xb8, 0xe8, 0xa6, 0x9d, 0x8e, 0x97, 0x08, 0xf6 } };

char *aurix_get_cmdline()
{
	char *bootargs = NULL;
	EFI_UINTN size = 0;
	EFI_STATUS status;

	status = gSystemTable->RuntimeServices->GetVariable(
		L"boot-args", &bootargs_guid, NULL, &size, NULL);
	if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL) {
		debug(
			"aurix_get_cmdline(): Failed to retrieve boot-args length: %s (0x%llx)\n",
			efi_status_to_str(status), status);
		return NULL;
	}

	bootargs = mem_alloc((size_t)size);
	if (!bootargs) {
		debug(
			"aurix_get_cmdline(): Failed to allocate memory for boot arguments!\n");
		return NULL;
	}

	status = gSystemTable->RuntimeServices->GetVariable(
		L"boot-args", &bootargs_guid, NULL, &size, bootargs);
	if (EFI_ERROR(status)) {
		debug("aurix_get_cmdline(): Failed to fetch boot-args: %s (0x%llx)\n",
			  efi_status_to_str(status), status);
		mem_free(bootargs);
		return NULL;
	}

	return bootargs;
}
#endif

bool aurix_get_framebuffer(struct aurix_parameters *params)
{
	struct fb_mode *modes;
	int current_mode;
	uint32_t *fb_addr;

	if (!get_framebuffer(&fb_addr, &modes, NULL, &current_mode)) {
		error(
			"aurix_get_framebuffer(): get_framebuffer() returned false, setting everything to 0.\n");
		return false;
	}

	params->framebuffer.addr = (uintptr_t)fb_addr;
	params->framebuffer.width = modes[current_mode].width;
	params->framebuffer.height = modes[current_mode].height;
	params->framebuffer.bpp = modes[current_mode].bpp;
	params->framebuffer.pitch = modes[current_mode].pitch;
	params->framebuffer.format = modes[current_mode].format;

	return true;
}

void aurix_load(char *kernel_path)
{
	char *kbuf = NULL;
	vfs_read(kernel_path, &kbuf, NULL);

	pagetable *pm = create_pagemap();
	if (!pm) {
		error("aurix_load(): Failed to create kernel pagemap! Halting...\n");
		// TODO: Halt
		while (1)
			;
	}

	axboot_memmap *mmap;
	uint32_t mmap_entries = get_memmap(&mmap, pm);

	map_pages(pm, (uintptr_t)pm, (uintptr_t)pm, PAGE_SIZE,
			  VMM_PRESENT | VMM_WRITABLE);

	void *stack = mem_alloc(
		AURIX_STACK_SIZE); // 16 KiB stack should be well more than enough
	if (!stack) {
		error("aurix_load(): Failed to allocate stack! Halting...\n");
		while (1)
			;
	}
	memset(stack, 0, AURIX_STACK_SIZE);

	map_pages(pm, (uintptr_t)stack, (uintptr_t)stack, AURIX_STACK_SIZE,
			  VMM_PRESENT | VMM_WRITABLE | VMM_NX);

	uintptr_t kernel_addr = 0;
	size_t kernel_size = 0;
	void *kernel_entry = (void *)elf_load(kbuf, &kernel_addr, &kernel_size, pm);
	if (!kernel_entry) {
		error("aurix_load(): Failed to load '%s'! Halting...\n", kernel_path);
		while (1)
			;
	}
	mem_free(kbuf);

	struct aurix_parameters parameters = { 0 };

	// set current revision
	parameters.revision = AURIX_PROTOCOL_REVISION;
	parameters.stack_addr = (uintptr_t)stack;

	parameters.kernel_addr = kernel_addr;
	parameters.hhdm_offset = 0xffff800000000000;

	// get boot arguments
#ifdef AXBOOT_UEFI
	parameters.cmdline = aurix_get_cmdline();
#else
#warning Passing boot arguments is supported on UEFI only
	parameters.cmdline = NULL;
#endif

	// get framebuffer information
	if (!aurix_get_framebuffer(&parameters)) {
		error("aurix_load(): Failed to aqcuire framebuffer information!\n");
	}

	// map framebuffer
	parameters.framebuffer.addr += parameters.hhdm_offset;
	map_pages(pm, parameters.framebuffer.addr,
			  parameters.framebuffer.addr - parameters.hhdm_offset,
			  parameters.framebuffer.height * parameters.framebuffer.pitch,
			  VMM_PRESENT | VMM_WRITABLE);

	// translate memory map
	if (!aurix_get_memmap(&parameters, kernel_size, mmap, mmap_entries, pm)) {
		error("aurix_load(): Failed to aqcuire memory map!");
		while (1)
			;
	}

	// map usable mmap entries to hhdm too
	for (uint32_t i = 0; i < parameters.mmap_entries; i++) {
		struct aurix_memmap *e = &parameters.mmap[i];
		if (e->type == AURIX_MMAP_RESERVED)
			continue;

		map_pages(pm, e->base + parameters.hhdm_offset, e->base, e->size,
				  VMM_PRESENT | VMM_WRITABLE);
	}

	// get RSDP and SMBIOS
#ifdef ARCH_ACPI_AVAILABLE
	parameters.rsdp_addr = platform_get_rsdp();
#endif

#ifdef ARCH_SMBIOS_AVAILABLE
	parameters.smbios_addr = platform_get_smbios();
#endif

	// load init ramdisk
	char *ramdisk = NULL;
	size_t ramdisk_size = 0;
	vfs_read("\\System\\ramdisk", &ramdisk, &ramdisk_size);
	parameters.ramdisk.addr = (void *)(ramdisk + parameters.hhdm_offset);
	parameters.ramdisk.size = ramdisk_size;

	debug(
		"aurix_load(): Handoff state: pm=0x%llx, stack=0x%llx, kernel_entry=0x%llx\n",
		pm, stack, kernel_entry);
#ifdef AXBOOT_UEFI
	uefi_exit_bs();
#endif

	aurix_arch_handoff(kernel_entry, pm, stack, AURIX_STACK_SIZE, &parameters);
	__builtin_unreachable();
}
