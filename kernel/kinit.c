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
#include <acpi/acpi.h>
#include <boot/args.h>
#include <cpu/cpu.h>
#include <debug/uart.h>
#include <arch/cpu/irq.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <smbios/smbios.h>
#include <lib/string.h>
#include <aurix.h>
#include <platform/time/rtc.h>
#include <flanterm/flanterm.h>
#include <flanterm/backends/fb.h>
#include <stddef.h>
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

/* ======== TESTS ======== */
#if 0
typedef void (*test_func_t)(void);
typedef unsigned int uint32_t;

typedef struct {
	const char *name;
	test_func_t func;
	uint32_t pass_count;
	uint32_t fail_count;
	uint32_t current;
} test_case_t;

#define TEST_MAX 3
static test_case_t test_suite[TEST_MAX];
static uint32_t test_count = 0;

#define TEST_ADD(fn)                                                        \
	do {                                                                    \
		if (test_count < TEST_MAX) {                                        \
			test_suite[test_count].name = #fn;                              \
			test_suite[test_count].func = fn;                               \
			test_suite[test_count].pass_count = 0;                          \
			test_suite[test_count].fail_count = 0;                          \
			test_suite[test_count].current = 0;                             \
			test_count++;                                                   \
		} else {                                                            \
			warn("Test suite overflow: cannot add %s, max %d tests\n", #fn, \
				 TEST_MAX);                                                 \
		}                                                                   \
	} while (0)

#define TEST_EXPECT(condition)                      \
	do {                                            \
		if (!(condition)) {                         \
			test_suite[test_count - 1].current = 1; \
		}                                           \
	} while (0)

static void test_run(uint32_t loop_count)
{
	debug("Starting test suite with %u loops, %u tests registered\n",
		  loop_count, test_count);
	if (test_count == 0) {
		warn("No tests registered to run\n");
		return;
	}
	for (uint32_t i = 0; i < test_count; i++) {
		test_suite[i].pass_count = 0;
		test_suite[i].fail_count = 0;
		test("Running test: %s\n", test_suite[i].name);
		for (uint32_t j = 0; j < loop_count; j++) {
			test_suite[i].current = 0;
			test_suite[i].func();
			if (test_suite[i].current == 0) {
				test_suite[i].pass_count++;
				success("Test %s (run %u): Passed\n", test_suite[i].name,
						j + 1);
			} else {
				test_suite[i].fail_count++;
				error("Test %s (run %u): Failed\n", test_suite[i].name, j + 1);
			}
		}
		test("Test %s: %u passes, %u fails\n", test_suite[i].name,
			 test_suite[i].pass_count, test_suite[i].fail_count);
	}
	test_count = 0;
	debug("Test suite completed and cleared\n");
}

static void pmm_test(void)
{
	char *test = palloc(1);
	TEST_EXPECT(test != NULL);
	if (test)
		pfree(test, 1);
}

static void heap_test(void)
{
	char *test = kmalloc(1024);
	TEST_EXPECT(test != NULL);
	if (test) {
		*test = 'a';
		kfree(test);
	}
}
#else
#define TEST_ADD(x)
#define test_run(x)
#endif
/* ====================== */

void hello(void)
{
	info("Hello from thread: %d!\n", 0);
	return;
}

// FIXME: local variables inside this function are behaving weird
rtc_time_t time;
rtc_error_t rtc_err;
pcb *test_proc;

void _start(struct aurix_parameters *params)
{
	boot_params = params;
	hhdm_offset = params->hhdm_offset;
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
	TEST_ADD(pmm_test);
	test_run(10);

	paging_init();

	smbios_init((void *)boot_params->smbios_addr);

	acpi_init((void *)boot_params->rsdp_addr);
	apic_init();

	cpu_init();

	debug("kernel cmdline: %s\n", boot_params->cmdline);
	parse_boot_args(boot_params->cmdline);

	heap_init(vinit(kernel_pm, 0x1000));
	TEST_ADD(heap_test);
	test_run(10);

	cpu_init_mp();

	sched_init();
	test_proc = proc_create();
	proc_destroy(test_proc);

	pmm_reclaim_bootparms();

	// finished with init, now just display fancy stuff
	kprintf("--------------------------------------------------\n");

	rtc_err = rtc_init();
	if (rtc_err != RTC_OK) {
		error("RTC init failed: %d\n", rtc_err);
	}

	rtc_err = rtc_get_time(&time);
	if (rtc_err != RTC_OK) {
		error("RTC get time failed: %d\n", rtc_err);
	}

	info("Current time: %04d-%02d-%02d %02d:%02d:%02d\n", time.year, time.month,
		 time.day, time.hours, time.minutes, time.seconds);
	info("Kernel boot complete in ? seconds\n");

	for (;;) {
#ifdef __x86_64__
		__asm__ volatile("cli;hlt");
#elif __aarch64__
		__asm__ volatile("wfe");
#endif
	}
}