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
#include <debug/uart.h>
#include <debug/print.h>
#include <stddef.h>
#include <mm/pmm.h>

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
	klog("%d memmap entries\n", boot_params->mmap_entries);
	for (int i = 0; i < boot_params->mmap_entries; i++) {
		struct aurix_memmap *m = &boot_params->mmap[i];
		klog("mmap[%d]: type=%d, start=%p, end=%p, size=%llu\n", i, m->type,
			 m->base, m->base + m->size, m->size);
	}
	pmm_init();
	char *a = palloc(1, false);
	*a = 69;
	klog("Allocated 1 page @ 0x%p\n", a);

	for (;;) {
#ifdef __x86_64__
		__asm__ volatile("cli;hlt");
#elif __aarch64__
		__asm__ volatile("wfe");
#endif
	}
}
