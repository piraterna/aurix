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

// #include <driver.h>
#include <config/config.h>
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
	if (!verify_secure_boot()) {
		debug("load_drivers(): Secure boot is enabled! Won't load drivers...\n");
		return;
	}
}