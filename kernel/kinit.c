/*********************************************************************************/
/* Module Name:  kinit.c */
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

#include <boot/aurix.h>
#include <acpi/acpi.h>
#include <cpu/cpu.h>
#include <debug/uart.h>
#include <debug/log.h>
#include <stddef.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <lib/string.h>
#include <flanterm/flanterm.h>
#include <flanterm/backends/fb.h>

struct aurix_parameters *boot_params = NULL;
struct flanterm_context *ft_ctx = NULL;

void _start(struct aurix_parameters *params)
{
	boot_params = params;
	serial_init();

	if (params->revision != AURIX_PROTOCOL_REVISION) {
		error(
			"Aurix Protocol revision is not compatible: expected %u, but got %u!\n",
			AURIX_PROTOCOL_REVISION, params->revision);
		for (;;)
			;
	}

	ft_ctx = flanterm_fb_init(
		NULL, NULL, (uint32_t *)boot_params->framebuffer.addr,
		boot_params->framebuffer.width, boot_params->framebuffer.height,
		boot_params->framebuffer.pitch, 8, 16, // red_mask_size, red_mask_shift
		8, 8, // green_mask_size, green_mask_shift
		8, 0, // blue_mask_size, blue_mask_shift
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 1, 0, 0, 0);

	if (!ft_ctx)
		error("failed to init flanterm\n");

	info("Hello from AurixOS!\n");

	// initialize basic processor features and interrupts
	cpu_early_init();

	// initialize memory stuff
	pmm_init();
	paging_init();

	acpi_init((void *)boot_params->rsdp_addr);

	// this should be called when we don't need boot parameters anymore
	pmm_reclaim_bootparms();

	// TODO: Track kernel boot time
	info("Kernel boot complete in 0 seconds\n");

	for (;;) {
#ifdef __x86_64__
		__asm__ volatile("cli;hlt");
#elif __aarch64__
		__asm__ volatile("wfe");
#endif
	}
}
