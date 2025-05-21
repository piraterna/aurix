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

#include <driver.h>
#include <config/config.h>
#include <mm/mman.h>
#include <lib/string.h>
#include <vfs/vfs.h>
#include <print.h>

#include <efi.h>
#include <efilib.h>
#include <stdbool.h>

bool verify_secure_boot()
{
	EFI_GUID var_guid = EFI_GLOBAL_VARIABLE;
	EFI_UINT8 val = 0;
	EFI_UINTN size = sizeof(val);
	bool ret = 0;

	if (!EFI_ERROR(gSystemTable->RuntimeServices->GetVariable(L"SecureBoot", &var_guid, NULL, &size, &val))) {
		debug("verify_secure_boot(): Secure Boot Status: %u\n", val);
		ret = (bool)val;

		if (!EFI_ERROR(gSystemTable->RuntimeServices->GetVariable(L"SetupMode", &var_guid, NULL, &size, &val)) && val != 0) {
			ret = false;
		}
	}

	return !ret;
}

void load_drivers()
{
	EFI_STATUS status;
	if (!verify_secure_boot()) {
		debug("load_drivers(): Secure boot is enabled! Won't load drivers...\n");
		return;
	}

	// TODO: Create a vfs_list() function to get a list of files in a directory
	// char *driver_path = "\\AxBoot\\drivers\\.efi";
	// char *driver_binary;
	// debug("load_drivers(): Loading '%s'...\n", driver_name);
	//
	// size_t driver_size = vfs_read(audio_path, &driver_binary);
	//
	// audio.devpath[0].Header.Length[0] = sizeof(EFI_MEMMAP_DEVICE_PATH);
    // audio.devpath[0].Header.Length[1] = sizeof(EFI_MEMMAP_DEVICE_PATH) >> 8;
    // audio.devpath[0].Header.Type = 1;
    // audio.devpath[0].Header.SubType = 3;
    // audio.devpath[0].MemoryType = EfiLoaderData;
    // audio.devpath[0].StartingAddress = (EFI_UINTPTR)driver_binary;
    // audio.devpath[0].EndingAddress = (EFI_UINTPTR)driver_binary + driver_size;
    // audio.devpath[1].Header.Length[0] = sizeof(EFI_DEVICE_PATH_PROTOCOL);
    // audio.devpath[1].Header.Length[1] = sizeof(EFI_DEVICE_PATH_PROTOCOL) >> 8;
    // audio.devpath[1].Header.Type = 0x7F;
    // audio.devpath[1].Header.SubType = 0xFF;
	//
	// status = gSystemTable->BootServices->LoadImage(EFI_FALSE, gImageHandle, (EFI_DEVICE_PATH_PROTOCOL *)driver_devpath, driver_binary, driver_size, &driver_handle);
	// if (EFI_ERROR(status)) {
		// debug("load_drivers(): Failed to load driver '%s': %s (%llx)\n", driver_name, efi_status_to_str(status), status);
		// return;
	// }
	// 
	// status = gSystemTable->BootServices->StartImage(driver_handle, NULL, NULL);
	// if (EFI_ERROR(status)) {
		// debug("load_drivers(): Failed to start driver '%s': %s (%llx)\n", driver_name, efi_status_to_str(status), status);
		// return;
	// }
}