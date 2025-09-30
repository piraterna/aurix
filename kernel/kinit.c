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
#include <cpu/cpu.h>
#include <arch/cpu/cpu.h>
#include <debug/uart.h>
#include <debug/print.h>
#include <stddef.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

struct aurix_parameters *boot_params = NULL;

void _start(struct aurix_parameters *params)
{
	boot_params = params;
	serial_init();

	if (params->revision != AURIX_PROTOCOL_REVISION) {
		klog(
			"Aurix Protocol revision is not compatible: expected %u, but got %u!\n",
			AURIX_PROTOCOL_REVISION, params->revision);
	}

	klog("Hello from AurixOS!\n");

	// initialize basic processor features and interrupts
	cpu_early_init();

	// initialize memory stuff
	pmm_init();
	paging_init();

	// this should be called when we don't need boot parameters anymore
	pmm_reclaim_bootparms();

	// TODO: Track kernel boot time
	klog("Kernel boot complete in 0 seconds\n");

	vctx_t *vctx = vinit(kernel_pm, VPM_MIN_ADDR);
	char *a = valloc(vctx, 1, VALLOC_RW);
	klog("Allocated 1 virtual page @ %p\n", a);

	for (;;) {
#ifdef __x86_64__
		__asm__ volatile("cli;hlt");
#elif __aarch64__
		__asm__ volatile("wfe");
#endif
	}
}
