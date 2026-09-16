/*********************************************************************************/
/* Module Name:  kinit.c */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#include <boot/axprot.h>
#include <boot/args.h>

#include <arch/cpu/cpu.h>
#include <arch/apic/apic.h>
#include <arch/cpu/irq.h>
#include <cpu/cpu.h>

#include <debug/log.h>
#include <debug/uart.h>

#include <acpi/acpi.h>
#include <smbios/smbios.h>

#include <mm/pmm.h>

#include <platform/time/pit.h>
#include <platform/time/time.h>

#include <aurix.h>

struct aurix_parameters *boot_params = NULL;
uintptr_t hhdm_offset = 0;

void _start(struct aurix_parameters *params)
{
	boot_params = params;
	hhdm_offset = params->hhdm_offset;

	log_init();
	serial_init();

	if (params->revision != AURIX_PROTOCOL_REVISION) {
		kpanicf(NULL, "Aurix Protocol revision mismatch: expected %u, got %u",
				AURIX_PROTOCOL_REVISION, params->revision);
	}

	cpu_early_init();

	pmm_init();
	paging_init();

	acpi_init((void *)boot_params->rsdp_addr);
	smbios_init((void *)boot_params->smbios_addr);

	apic_init();
	cpu_init();
	cpu_init_mp();

	platform_timekeeper_init();

	// we don't need it now
	pmm_reclaim_bootparms();

	uint64_t ms = get_ms();
	success("Kernel boot complete in %u.%03u seconds\n",
			(uint32_t)(ms / 1000ull), (uint32_t)(ms % 1000ull));
	success("Running at %d cores on an %s\n", cpu_count,
			cpu_get_current()->name_ext);

	for (;;) {
#ifdef __x86_64__
		__asm__ volatile("hlt");
#elif __aarch64__
		__asm__ volatile("wfe");
#endif
	}

	UNREACHABLE();
}
