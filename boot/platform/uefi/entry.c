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
#include <print.h>

#include <stdint.h>
#include <stddef.h>

#define INI_IMPLEMENTATION
#include <config/ini.h>


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
