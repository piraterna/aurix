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
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#include <boot/axprot.h>
#include <arch/cpu/cpu.h>
#include <arch/apic/apic.h>
#include <arch/cpu/irq.h>
#include <acpi/acpi.h>
#include <boot/args.h>
#include <cpu/cpu.h>
#include <debug/uart.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <loader/module.h>
#include <smbios/smbios.h>
#include <time/time.h>
#include <lib/string.h>
#include <platform/time/pit.h>
#include <platform/time/time.h>
#include <dev/driver.h>
#include <dev/devfs.h>
#include <flanterm/flanterm.h>
#include <flanterm/backends/fb.h>
#include <test/test.h>
#include <aurix.h>
#include <sys/sched.h>

struct aurix_parameters *boot_params = NULL;
struct flanterm_context *ft_ctx = NULL;

uintptr_t hhdm_offset = 0;

const char *aurix_banner =
	"    _              _       ___  ____\n"
	"   / \\  _   _ _ __(_)_  __/ _ \\/ ___|\n"
	"  / _ \\| | | | '__| \\ \\/ / | | \\___ \\\n"
	" / ___ \\ |_| | |  | |>  <| |_| |___) |\n"
	"/_/   \\_\\__,_|_|  |_/_/ \\_\\___/|____/  (c) Copyright 2024-2025 Jozef Nagy";

void hello(void)
{
	kprintf("hello from a thread %d!\n", thread_current()->tid);
	thread_exit(thread_current());
	for (;;)
		;
}

// FIXME: local variables inside this function are behaving weird
vctx_t *kvctx;
int i;

void _start(struct aurix_parameters *params)
{
	boot_params = params;
	hhdm_offset = params->hhdm_offset;
	log_init();
	serial_init();

	if (params->revision != AURIX_PROTOCOL_REVISION) {
		critical(
			"Aurix Protocol revision is not compatible: expected %u, but got %u!\n",
			AURIX_PROTOCOL_REVISION, params->revision);
		for (;;)
			;
	}

	ft_ctx = flanterm_fb_init(
		NULL, NULL, (uint32_t *)boot_params->framebuffer.addr,
		boot_params->framebuffer.width, boot_params->framebuffer.height,
		boot_params->framebuffer.pitch, 8, 16, 8, 8, 8, 0, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, 0, 0, 1, 0, 0, 0);

	if (!ft_ctx)
		error("Failed to init flanterm\n");

	kprintf("%s\n", aurix_banner);
	info("Hello from AurixOS!\n");

	cpu_early_init();

	pmm_init();

	paging_init();

	smbios_init((void *)boot_params->smbios_addr);

	acpi_init((void *)boot_params->rsdp_addr);
	apic_init();

	cpu_init();

	debug("kernel cmdline: %s\n", boot_params->cmdline);
	parse_boot_args(boot_params->cmdline);

	kvctx = vinit(kernel_pm, 0xffffffff90000000ULL);
	heap_init(kvctx);
	driver_core_init();
	devfs_init();

	// TODO: Add kernel cmdline parsing
	if (1) {
		test_run(10);
	}

#ifdef __x86_64__
	// TODO: Use HPET instead?
	pit_init(50);
#else
#warning No clock implemented, the scheduler will not fire!
#endif

	sched_init();
	cpu_init_mp();

	platform_timekeeper_init();

	for (uint32_t m = 0; m < boot_params->module_count; m++) {
		info("Loading module '%s'...\n", boot_params->modules[m].filename);
		if (!module_load(boot_params->modules[m].addr,
						 boot_params->modules[m].size)) {
			error("Driver '%s' failed to load");
			cpu_halt();
		}
	}

	info("Current time: %04d-%02d-%02d %02d:%02d:%02d\n", time_get_year(),
		 time_get_month(), time_get_day(), time_get_hour(), time_get_minute(),
		 time_get_second());

	pmm_reclaim_bootparms();
	{
		uint64_t ms = log_uptime_ms();
		info("Kernel boot complete in %u.%03u seconds\n",
			 (uint32_t)(ms / 1000ull), (uint32_t)(ms % 1000ull));
	}

	sched_enable();

	for (;;) {
#ifdef __x86_64__
		__asm__ volatile("hlt");
#elif __aarch64__
		__asm__ volatile("wfe");
#endif
	}

	UNREACHABLE();
}
