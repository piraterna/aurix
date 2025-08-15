/*********************************************************************************/
/* Module Name:  acpi.c                                                          */
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

#include <lib/string.h>
#include <print.h>
#include <stdint.h>

#include <efi.h>
#include <efilib.h>

uintptr_t platform_get_rsdp()
{
	EFI_GUID acpi10_guid = EFI_ACPI_10_TABLE_GUID;
	EFI_GUID acpi20_guid = EFI_ACPI_20_TABLE_GUID;
	uintptr_t rsdp_addr = 0;

	for (EFI_UINTN i = 0; i < gSystemTable->NumberOfTableEntries; i++) {
		if (memcmp(&(gSystemTable->ConfigurationTable[i].VendorGuid), &acpi10_guid, sizeof(EFI_GUID)) == 0) {
			debug("platform_get_rsdp(): Found RSDP (ACPI 1.0) at 0x%llx\n", gSystemTable->ConfigurationTable[i].VendorTable);
			rsdp_addr = (uintptr_t)gSystemTable->ConfigurationTable[i].VendorTable;
		}

		// ACPI 2.0+ always takes higher priority over older version
		if (memcmp(&(gSystemTable->ConfigurationTable[i].VendorGuid), &acpi20_guid, sizeof(EFI_GUID)) == 0) {
			debug("platform_get_rsdp(): Found RSDP (ACPI 2.0+) at 0x%llx\n", gSystemTable->ConfigurationTable[i].VendorTable);
			return (uintptr_t)gSystemTable->ConfigurationTable[i].VendorTable;
		}
	}

	if (rsdp_addr == 0) {
		debug("platform_get_rsdp(): RSDP not found!\n");
	}

	return rsdp_addr;
}

uintptr_t platform_get_smbios()
{
	EFI_GUID smbios_guid = SMBIOS_TABLE_GUID;
	EFI_GUID smbios3_guid = SMBIOS3_TABLE_GUID;
	uintptr_t smbios_addr = 0;

	for (EFI_UINTN i = 0; i < gSystemTable->NumberOfTableEntries; i++) {
		if (memcmp(&(gSystemTable->ConfigurationTable[i].VendorGuid), &smbios_guid, sizeof(EFI_GUID)) == 0) {
			debug("platform_get_rsmbios(): Found SMBIOS at 0x%llx\n", gSystemTable->ConfigurationTable[i].VendorTable);
			smbios_addr = (uintptr_t)gSystemTable->ConfigurationTable[i].VendorTable;
		}

		// SMBIOS3 always takes higher priority over older version
		if (memcmp(&(gSystemTable->ConfigurationTable[i].VendorGuid), &smbios3_guid, sizeof(EFI_GUID)) == 0) {
			debug("platform_get_smbios(): Found SMBIOS3 at 0x%llx\n", gSystemTable->ConfigurationTable[i].VendorTable);
			return (uintptr_t)gSystemTable->ConfigurationTable[i].VendorTable;
		}
	}

	if (smbios_addr == 0) {
		debug("platform_get_smbios(): SMBIOS not found!\n");
	}

	return smbios_addr;
}