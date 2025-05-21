/*********************************************************************************/
/* Module Name:  datetime.c                                                      */
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

#include <time/dt.h>
#include <stdint.h>
#include <stddef.h>
#include <axboot.h>
#include <print.h>

#include <efi.h>
#include <efilib.h>

void get_datetime(struct datetime *dt)
{
	EFI_STATUS status;
	EFI_TIME uefi_dt;

	status = gSystemTable->RuntimeServices->GetTime(&uefi_dt, NULL);
	if (EFI_ERROR(status)) {
		debug("get_datetime(): Failed to acquire current time, setting to 1970/01/01 00:00:00!\n");
		return;
	}

	dt->year = uefi_dt.Year;
	dt->month = uefi_dt.Month;
	dt->day = uefi_dt.Day;
	dt->h = uefi_dt.Hour;
	dt->m = uefi_dt.Minute;
	dt->s = uefi_dt.Second;
}
