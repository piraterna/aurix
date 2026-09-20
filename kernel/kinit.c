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
#include <sys/spinlock.h>

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

#if CONFIG_KCONSOLE == 1
#include <flanterm.h>
#include <flanterm_backends/fb.h>

struct flanterm_context *ft_ctx;
static spinlock_t kcon_lock;

static inline const char *_e_find_nl(const char *p, const char *end)
{
	for (; p < end; p++)
		if (*p == '\n')
			return p;
	return NULL;
}

void _e_kcon_puts(const char *str, size_t len)
{
	if (ft_ctx == NULL || str == NULL || len == 0)
		return;

	spinlock_acquire(&kcon_lock);
	const char *p = str;
	const char *end = str + len;

	while (p < end) {
		const char *nl = _e_find_nl(p, end);

		if (nl == NULL) {
			flanterm_write(ft_ctx, p, (size_t)(end - p));
			break;
		}

		flanterm_write(ft_ctx, p, (size_t)(nl - p) + 1);

		if (nl == str || nl[-1] != '\r')
			flanterm_write(ft_ctx, "\r", 1);

		p = nl + 1;
	}
	spinlock_release(&kcon_lock);
}
#endif

void _start(struct aurix_parameters *params)
{
	boot_params = params;
	hhdm_offset = params->hhdm_offset;
	spinlock_init(&kcon_lock);

	log_init();
	serial_init();

	if (params->revision != AURIX_PROTOCOL_REVISION) {
		kpanicf(NULL, "Aurix Protocol revision mismatch: expected %u, got %u",
				AURIX_PROTOCOL_REVISION, params->revision);
	}

	success("    _              _\n");
	success("   / \\  _   _ _ __(_)_  __\n");
	success("  / _ \\| | | | '__| \\ \\/ /\n");
	success(" / ___ \\ |_| | |  | |>  <\n");
	success("/_/   \\_\\__,_|_|  |_/_/\\_\\\n");
	success("\n");
	success("Booted Aurix kernel\n");

#if CONFIG_KCONSOLE == 1
	uint32_t red_size = 8, green_size = 8, blue_size = 8;
	uint32_t red_shift = 16, green_shift = 8, blue_shift = 0;

	uint32_t bpp = params->framebuffer.bpp * 8; // convert to bits

	if (bpp != 32 && bpp != 24) {
		kpanicf(NULL, "Unsupported framebuffer format: %u bpp\n", bpp);
	}

	if (params->framebuffer.format == AURIX_FB_BGRA) {
		red_shift = 16;
		green_shift = 8;
		blue_shift = 0;
	} else if (params->framebuffer.format != AURIX_FB_RGBA) {
		kpanicf(NULL, "Unsupported framebuffer pixel format: %d\n",
				params->framebuffer.format);
	}

	ft_ctx = flanterm_fb_init(
		NULL, NULL, (uint32_t *)params->framebuffer.addr,
		params->framebuffer.width, params->framebuffer.height,
		params->framebuffer.pitch, red_size, red_shift, green_size, green_shift,
		blue_size, blue_shift, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		0, 0, 1, 0, 0, 0, 0, true);

	if (!ft_ctx)
		error("Failed to initialize kconsole\n");
	else
		success("kconsole initialized\n");
#endif

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
