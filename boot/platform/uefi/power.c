/*********************************************************************************/
/* Module Name:  power.c                                                         */
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

#include <power.h>
#include <efi.h>
#include <efilib.h>
#include <axboot.h>
#include <print.h>

#include <stdbool.h>

bool platform_is_reboot_to_fw_possible()
{
	EFI_STATUS Status;
	EFI_UINT64 OsIndications;
	EFI_UINTN OsIndicationsSize = sizeof(EFI_UINT64);
	EFI_GUID GlobalVarGuid = EFI_GLOBAL_VARIABLE;

	Status = gSystemTable->RuntimeServices->GetVariable(L"OsIndicationsSupported", &GlobalVarGuid, NULL, &OsIndicationsSize, &OsIndications);
	if (!EFI_ERROR(Status)) {
		if (OsIndications & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) {
			debug("platform_is_reboot_to_fw_possible(): Boot to firmware UI is possible!\n");
			return true;
		}
	} else {
		debug("platform_is_reboot_to_fw_possible(): Failed to get OsIndications variable: 0x%llx (%s) \n", Status, efi_status_to_str(Status));
	}

	return false;
}

void platform_reboot_to_fw()
{
	if (!platform_is_reboot_to_fw_possible()) {
		return;
	}

	EFI_STATUS Status;
	EFI_UINT64 OsIndications = EFI_OS_INDICATIONS_BOOT_TO_FW_UI;
	EFI_UINTN OsIndicationsSize = sizeof(EFI_UINT64);
	EFI_GUID GlobalVarGuid = EFI_GLOBAL_VARIABLE;

	Status = gSystemTable->RuntimeServices->SetVariable(L"OsIndications", &GlobalVarGuid, EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS, OsIndicationsSize, &OsIndications);
	if (EFI_ERROR(Status)) {
		debug("platform_reboot_to_fw(): Failed to set OsIndications variable: 0x%llx (%s) \n", Status, efi_status_to_str(Status));
	}

	gSystemTable->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
}

void platform_reboot()
{
	gSystemTable->RuntimeServices->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);
}

void platform_shutdown()
{
	gSystemTable->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
}