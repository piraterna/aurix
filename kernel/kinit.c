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
#include <vfs/vfs.h>
#include <flanterm/flanterm.h>
#include <flanterm/backends/fb.h>
#include <test/test.h>
#include <aurix.h>
#include <sys/sched.h>
#include <sys/panic.h>

struct aurix_parameters *boot_params = NULL;
struct flanterm_context *ft_ctx = NULL;

// ======= Flanterm theme, just for looks (kernel log, pre-TTY) =======
static uint32_t ft_ansi_colours[8] = {
	0x00101114, /* black */
	0x008b2e2e, /* red */
	0x003a7d44, /* green */
	0x008b6f1f, /* yellow */
	0x002f4f7f, /* blue */
	0x006a3f7a, /* magenta */
	0x002f6f73, /* cyan */
	0x00c5c8c6, /* white */
};

static uint32_t ft_ansi_bright_colours[8] = {
	0x00202024, /* bright black (dark gray) */
	0x00b84a4a, /* bright red */
	0x004fae66, /* bright green */
	0x00b8952e, /* bright yellow */
	0x004c78c4, /* bright blue */
	0x008c5fbf, /* bright magenta */
	0x004fb3b8, /* bright cyan */
	0x00e6e6e6, /* bright white */
};
static uint32_t ft_default_bg = 0x000d0f12; /* near-black */
static uint32_t ft_default_fg = 0x00d0d0d0; /* neutral light gray */
static uint32_t ft_default_bg_bright = 0x0015151a; /* slightly lifted */
static uint32_t ft_default_fg_bright = 0x00ffffff; /* clean white */
// ====================================================================

uintptr_t hhdm_offset = 0;
vctx_t *kvctx = NULL;

const char *aurix_banner =
	"    _              _       ___  ____\n"
	"   / \\  _   _ _ __(_)_  __/ _ \\/ ___|\n"
	"  / _ \\| | | | '__| \\ \\/ / | | \\___ \\\n"
	" / ___ \\ |_| | |  | |>  <| |_| |___) |\n"
	"/_/   \\_\\__,_|_|  |_/_/ \\_\\___/|____/  (c) Copyright 2024-2026 Jozef Nagy";

void hello(void)
{
	kprintf("hello from a kernel proccess. (tid=%d, pid=%d, cpu=%d)\n",
			thread_current()->tid, thread_current()->process->pid,
			cpu_get_current()->id);
	thread_exit(thread_current());
}

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

	ft_ctx = flanterm_fb_init(
		NULL, NULL, (uint32_t *)boot_params->framebuffer.addr,
		boot_params->framebuffer.width, boot_params->framebuffer.height,
		boot_params->framebuffer.pitch, 8, 16, 8, 8, 8, 0, NULL,
		ft_ansi_colours, ft_ansi_bright_colours, &ft_default_bg, &ft_default_fg,
		&ft_default_bg_bright, &ft_default_fg_bright, NULL, 0, 0, 1, 0, 0, 0);

	if (!ft_ctx)
		error("Failed to init flanterm\n");

	kprintf("%s\n", aurix_banner);

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

	cpu_init_mp();
	sched_init();

	platform_timekeeper_init();

	for (uint32_t m = 0; m < boot_params->module_count; m++) {
		trace("Loading module '%s'...\n", boot_params->modules[m].filename);
		if (!module_load(boot_params->modules[m].addr,
						 boot_params->modules[m].size)) {
			kpanicf(NULL, "Module '%s' failed to load",
					boot_params->modules[m].filename);
		}
	}

	debug("Current time: %04d-%02d-%02d %02d:%02d:%02d\n", time_get_year(),
		  time_get_month(), time_get_day(), time_get_hour(), time_get_minute(),
		  time_get_second());

	pmm_reclaim_bootparms();
	{
		uint64_t ms = get_ms();
		success("Kernel boot complete in %u.%03u seconds\n",
				(uint32_t)(ms / 1000ull), (uint32_t)(ms % 1000ull));
	}

	pcb *t = proc_create();
	thread_create(t, hello);

	pit_set_freq(1000); // 1kHz should be fast enough
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
