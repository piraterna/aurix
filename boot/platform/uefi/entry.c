/*********************************************************************************/
/* Module Name:  entry.c                                                         */
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

#include <efi.h>
#include <efilib.h>

#include <axboot.h>
#include <mm/mman.h>
#include <lib/string.h>
#include <power.h>
#include <print.h>

#include <stdint.h>
#include <stddef.h>

EFI_HANDLE gImageHandle;
EFI_SYSTEM_TABLE *gSystemTable;
EFI_BOOT_SERVICES *gBootServices;

EFI_SIMPLE_POINTER_PROTOCOL *gPointerProtocol;
uint16_t mouse_resx;
uint16_t mouse_resy;

EFI_STATUS uefi_entry(EFI_HANDLE ImageHandle,
					   EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS Status;
	EFI_GUID spp_guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
	EFI_SIMPLE_POINTER_PROTOCOL *spp[5];

	gImageHandle = ImageHandle;
	gSystemTable = SystemTable;
	gBootServices = SystemTable->BootServices;

	// reset input
	gSystemTable->ConIn->Reset(gSystemTable->ConIn, EFI_FALSE);

	// clear the screen
	gSystemTable->ConOut->ClearScreen(gSystemTable->ConOut);

	// disable UEFI watchdog
	Status = gSystemTable->BootServices->SetWatchdogTimer(0, 0, 0, NULL);
	if (EFI_ERROR(Status)) {
		debug("uefi_entry(): Couldn't disable UEFI watchdog: %s (%x)\n", efi_status_to_str(Status), Status);
	}

	// load that mouse up
	EFI_UINTN spp_handles = 0;
	EFI_HANDLE *spp_handle_buf = NULL;
	Status = gBootServices->LocateHandleBuffer(ByProtocol, &spp_guid, NULL, &spp_handles, &spp_handle_buf);
	if (EFI_ERROR(Status)) {
		debug("uefi_entry(): Failed to locate Simple Pointer Protocol handle buffer: %s (%x).", efi_status_to_str(Status), Status);
	} else {
		debug("uefi_entry(): Found %u handle%s\n", spp_handles, spp_handles == 1 ? "" : "s");
		for (EFI_UINTN i = 0; i < spp_handles; i++) {
			Status = gBootServices->OpenProtocol(spp_handle_buf[i], &spp_guid, (void **)&spp[i], gImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
			if (EFI_ERROR(Status)) {
				debug("uefi_entry(): Failed to open Simple Pointer Protocol on handle: %s (%x)\n", efi_status_to_str(Status), Status);
				continue;
			}

			debug("uefi_entry(): Found SPP with ResX=%u, ResY=%u\n", spp[i]->Mode->ResolutionX, spp[i]->Mode->ResolutionY);
			if (spp[i]->Reset(spp[i], EFI_TRUE) == EFI_DEVICE_ERROR) {
				debug("uefi_entry(): Failed to reset device\n");
				continue;
			}

			if (spp[i]->Mode->ResolutionX < 65536) {
				gPointerProtocol = spp[i];
				mouse_resx = spp[i]->Mode->ResolutionX;
				mouse_resy = spp[i]->Mode->ResolutionY;
				break;
			}
		}
	}

	axboot_init();
	UNREACHABLE();
}

#define EXIT_BS_MAX_TRIES 10

void uefi_exit_bs(void)
{
	EFI_UINTN map_key = 0;
	EFI_UINTN map_size = 0;
	EFI_MEMORY_DESCRIPTOR *map = NULL;
	EFI_UINTN desc_size = 0;
	EFI_UINT32 desc_ver = 0;
	EFI_STATUS status;
	
	int tries = 0;

	debug("uefi_exit_bs(): Calling ExitBootServices(), this will be the last log you'll see before handoff!\n");

	do {
		map_key = 0;
		map_size = 0;
		desc_size = 0;
		desc_ver = 0;
		map = NULL;

		tries++;
		debug("uefi_exit_bs(): Trying to exit Boot Services, try %u/%u...\n", tries, EXIT_BS_MAX_TRIES);

		status = gBootServices->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_ver);
		if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL) {
			debug("uefi_exit_bs(): Failed to acquire memory map key: %s (%llx)\n", efi_status_to_str(status), status);
			continue;
		}

		status = gBootServices->ExitBootServices(gImageHandle, map_key);
		if (EFI_ERROR(status)) {
			debug("uefi_exit_bs(): Failed to exit boot services: %s (%llx)\n", efi_status_to_str(status), status);
			continue;
		}

		break;
	} while (tries <= EXIT_BS_MAX_TRIES);

	if (EFI_ERROR(status)) {
		debug("uefi_exit_bs(): Failed to exit Boot Services, rebooting!");
		platform_reboot();
		UNREACHABLE();
	}
}
