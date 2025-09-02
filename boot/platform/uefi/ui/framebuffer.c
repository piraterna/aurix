/*********************************************************************************/
/* Module Name:  framebuffer.c */
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

#include <efi.h>
#include <efilib.h>
#include <mm/mman.h>
#include <print.h>
#include <ui/framebuffer.h>

#include <stddef.h>
#include <stdint.h>

bool get_framebuffer(uint32_t **fb_addr, struct fb_mode **available_modes,
					 int *total_modes, int *current_mode_index)
{
	EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info = NULL;
	EFI_UINTN mode_info_size = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
	EFI_UINTN SizeOfInfo;
	EFI_UINTN numModes = 0;
	EFI_UINTN nativeMode = 0;
	EFI_UINTN mode_index = 0;
	EFI_STATUS Status;

	Status = gBootServices->LocateProtocol(&gop_guid, NULL, (void **)&gop);
	if (EFI_ERROR(Status)) {
		debug("get_framebuffer(): Unable to locate GOP: %s (0x%llx)\n",
			  efi_status_to_str(Status), Status);
	}

	Status = gop->QueryMode(gop, gop->Mode == NULL ? 0 : gop->Mode->Mode,
							&SizeOfInfo, &mode_info);
	// this is needed to get the current video mode
	if (Status == EFI_NOT_STARTED) {
		Status = gop->SetMode(gop, 0);
	} else if (EFI_ERROR(Status)) {
		debug("Unable to get native mode\n");
		return false;
	} else {
		nativeMode = gop->Mode->Mode;
		numModes = gop->Mode->MaxMode;
	}

	if (total_modes != NULL) {
		*total_modes = numModes;
	}
	*available_modes =
		(struct fb_mode *)mem_alloc(sizeof(struct fb_mode) * numModes);

	*fb_addr = (uint32_t *)gop->Mode->FrameBufferBase;

	// get all available modes
	for (int i = 0; i < (int)numModes; i++) {
		Status = gop->QueryMode(gop, i, &SizeOfInfo, &mode_info);

		(*available_modes)[i].width = mode_info->HorizontalResolution;
		(*available_modes)[i].height = mode_info->VerticalResolution;
		(*available_modes)[i].bpp = 4;
		(*available_modes)[i].pitch = mode_info->PixelsPerScanLine * 4;

		if (mode_info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
			(*available_modes)[i].format = FB_RGBA;
		} else if (mode_info->PixelFormat ==
				   PixelBlueGreenRedReserved8BitPerColor) {
			(*available_modes)[i].format = FB_BGRA;
		} else {
			debug(
				"get_framebuffer(): Unknown framebuffer format, assuming BGRA...\n");
			(*available_modes)[i].format = FB_BGRA;
		}

		if (i == (int)nativeMode) {
			*current_mode_index = i;
		}
	}

	gop->QueryMode(gop, mode_index, &mode_info_size, &mode_info);

	return true;
}