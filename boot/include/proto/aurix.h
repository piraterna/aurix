/*********************************************************************************/
/* Module Name:  aurix.h */
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

#ifndef _AURIX_H
#define _AURIX_H

#include <stdint.h>

/* Aurix Boot Protocol (revision 1-dev) */
#define AURIX_PROTOCOL_REVISION 1

enum aurix_memmap_entry {
	AURIX_MMAP_RESERVED = 0,

	AURIX_MMAP_ACPI_RECLAIMABLE = 1,
	AURIX_MMAP_ACPI_MAPPED_IO = 2,
	AURIX_MMAP_ACPI_MAPPED_IO_PORTSPACE = 3,
	AURIX_MMAP_ACPI_NVS = 4,

	AURIX_MMAP_KERNEL = 5,

	AURIX_MMAP_FRAMEBUFFER = 6,
	AURIX_MMAP_BOOTLOADER_RECLAIMABLE = 7,
	AURIX_MMAP_USABLE = 10
};

enum aurix_framebuffer_format { AURIX_FB_RGBA = 0, AURIX_FB_BGRA = 1 };

struct aurix_memmap {
	uintptr_t base;
	uint32_t size;
	uint8_t type;
};

struct aurix_framebuffer {
	uintptr_t addr;
	uint32_t width;
	uint32_t height;
	uint8_t bpp; // bits!
	uint32_t pitch;
	int format;
};

struct aurix_module {
	char filename[32];
	uint8_t *addr;
	size_t size;
};

struct aurix_parameters {
	// PROTOCOL INFO
	uint8_t revision;

	// BOOT INFO
	char *cmdline;

	// MEMORY
	struct aurix_memmap *mmap;
	uint32_t mmap_entries;

	uintptr_t kernel_addr; // physical address
	uintptr_t hhdm_offset;
	uintptr_t stack_addr;

	// RSDP and SMBIOS
	uintptr_t rsdp_addr;
	uintptr_t smbios_addr;

	// initram
	struct aurix_module *modules;
	uint32_t module_count;

	// FRAMEBUFFER
	struct aurix_framebuffer framebuffer;
};

/* Kernel related stuff */
extern struct aurix_parameters *boot_params;

#ifdef _AXBOOT
#include <mm/vmm.h>
void aurix_load(char *kernel_path);
void aurix_arch_handoff(void *kernel_entry, pagetable *pm, void *stack,
						uint32_t stack_size,
						struct aurix_parameters *parameters);

#endif

#endif /* _PROTO_AURIX_H */
