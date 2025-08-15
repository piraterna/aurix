/*********************************************************************************/
/* Module Name:  memmap.c                                                        */
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

#include <mm/memmap.h>
#include <mm/mman.h>
#include <mm/vmm.h>
#include <lib/string.h>
#include <print.h>
#include <axboot.h>
#include <stddef.h>
#include <efi.h>
#include <efilib.h>

static int efi_type_to_axboot(uint32_t efi_type)
{
	switch (efi_type) {
		case EfiLoaderData:
		case EfiBootServicesCode:
		case EfiBootServicesData:
			return MemMapFreeOnLoad;

		case EfiRuntimeServicesCode:
		case EfiRuntimeServicesData:
		case EfiLoaderCode:
			return MemMapFirmware;
		
		case EfiUnusableMemory:
			return MemMapFaulty;

		case EfiACPIReclaimMemory:
			return MemMapACPIReclaimable;

		case EfiACPIMemoryNVS:
			return MemMapACPINVS;

		case EfiMemoryMappedIO:
			return MemMapACPIMappedIO;

		case EfiMemoryMappedIOPortSpace:
			return MemMapACPIMappedIOPortSpace;

		case EfiPalCode:
		case EfiPersistentMemory:
		case EfiConventionalMemory:
			return MemMapUsable;
			return MemMapUsable;

		case EfiUnacceptedMemoryType:
		case EfiReservedMemoryType:
			return MemMapReserved;
		default:
			return MemMapReserved;
	}
}

uint32_t get_memmap(axboot_memmap **map, pagetable *pm)
{
	EFI_MEMORY_DESCRIPTOR *efi_map = NULL;
	EFI_UINTN efi_map_key = 0;
	EFI_UINTN size = 0;
	EFI_UINTN desc_size = 0;
	EFI_UINT32 desc_ver = 0;
	EFI_STATUS status = 0;

	status = gBootServices->GetMemoryMap(&size, efi_map, &efi_map_key, &desc_size, &desc_ver);
	if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL) {
		debug("get_memmap(): GetMemoryMap() returned an error: %s (0x%llx)\n", efi_status_to_str(status), status);
		return 0;
	}

	efi_map = (EFI_MEMORY_DESCRIPTOR *)mem_alloc(size);

	do {
		status = gBootServices->GetMemoryMap(&size, efi_map, &efi_map_key, &desc_size, &desc_ver);
		if (!EFI_ERROR(status)) {
			break;
		} else if (status == EFI_BUFFER_TOO_SMALL) {
			size += 2 * desc_size;
			efi_map = (EFI_MEMORY_DESCRIPTOR *)mem_realloc(efi_map, size);
		} else {
			debug("get_memmap(): GetMemoryMap() returned an error: %s (0x%llx)\n", efi_status_to_str(status), status);
			return 0;
		}
	} while (status != EFI_SUCCESS);

	EFI_MEMORY_DESCRIPTOR *cur_entry = efi_map;
	uint32_t entry_count = size / desc_size;

	// map all the memory
	uint64_t flags;
	for (uint32_t i = 0; i < entry_count; i++) {
		flags = VMM_PRESENT;
		switch (cur_entry->Type) {
			case EfiConventionalMemory:
        	case EfiBootServicesCode:
        	case EfiBootServicesData:
			case EfiLoaderData:
			case EfiLoaderCode:
        	    flags |= VMM_WRITABLE;
        	    break;
        	case EfiACPIReclaimMemory:
        	case EfiACPIMemoryNVS:
        	case EfiMemoryMappedIO:
        	case EfiMemoryMappedIOPortSpace:
        	    flags |= VMM_WRITABLE | VMM_NX;
        	    break;
        	case EfiUnusableMemory:
        	case EfiReservedMemoryType:
        	case EfiPalCode:
			case EfiRuntimeServicesCode:
			case EfiRuntimeServicesData:
        	    flags = 0;
        	    break;
			default:
        	    flags |= VMM_WRITABLE | VMM_NX;
        	    break;
			}

		map_pages(pm, cur_entry->PhysicalStart, cur_entry->PhysicalStart, cur_entry->NumberOfPages * PAGE_SIZE, flags);
		cur_entry = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)cur_entry + desc_size);
	}

	*map = mem_alloc(sizeof(axboot_memmap) * entry_count);
	memset(*map, 0, sizeof(axboot_memmap) * entry_count);
	
	// translate efi memmap to axboot memmap
	for (uint32_t i = 0; i < entry_count; i++) {
		(*map)[i].base = cur_entry->PhysicalStart;
		(*map)[i].size = cur_entry->NumberOfPages * PAGE_SIZE;
		(*map)[i].type = efi_type_to_axboot(efi_map[i].Type);

		cur_entry = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)cur_entry + desc_size);
	}

	mem_free(efi_map);

	return entry_count;
}
