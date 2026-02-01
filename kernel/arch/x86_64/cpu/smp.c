/*********************************************************************************/
/* Module Name:  smp.c */
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

#include <arch/apic/apic.h>
#include <arch/cpu/cpu.h>
#include <cpu/cpu.h>
#include <arch/cpu/smp.h>
#include <arch/mm/paging.h>
#include <acpi/acpi.h>
#include <acpi/hpet.h>
#include <acpi/madt.h>
#include <cpu/cpu.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <sys/sched.h>
#include <aurix.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>

#define CORE_STARTUP_MAX_RETRIES 100 // 1s

bool smp_initialized = false;
static atomic_bool cpu_ready = ATOMIC_VAR_INIT(false);

static void *tramp_stack;

extern uintptr_t lapic_base;
extern struct madt_lapic *lapics[];
extern size_t lapic_count;

extern pagetable *kernel_pm;

void send_start_ipi(uint8_t cpu_id, uint8_t lapic_id)
{
	uint8_t *virt_base = (uint8_t *)TRAMP_BASE_ADDR;
	*((uint64_t *)&virt_base[TRAMP_CPU]) = (uint64_t)cpu_id;
	*((uint64_t *)&virt_base[TRAMP_STACK]) = (uint64_t)tramp_stack + PAGE_SIZE;

	atomic_store(&cpu_ready, false);

	// INIT
	lapic_write(0x310, lapic_id << 24);
	lapic_write(0x300, (5 << 8));

	hpet_msleep(10);

	// SIPI
	lapic_write(0x310, lapic_id << 24);
	lapic_write(0x300, (6 << 8) | ((uintptr_t)TRAMP_BASE_ADDR / PAGE_SIZE));
}

void smp_init()
{
	if (smp_initialized) {
		error("Tried to initialize SMP more than once!\n");
		return;
	}

	cpu_disable_interrupts();

	uint8_t bsp_id = cpu_get_current()->id;
	debug("Bootstrap CPU ID: %u\n", bsp_id);

	// set up trampoline
	tramp_stack = palloc(1);
	if (!tramp_stack) {
		error("Couldn't allocate memory for SMP trampoline stack.\n");
	}

	map_page(NULL, TRAMP_BASE_ADDR, TRAMP_BASE_ADDR,
			 VMM_PRESENT | VMM_WRITABLE);
	map_page(NULL, (uintptr_t)tramp_stack, (uintptr_t)tramp_stack,
			 VMM_PRESENT | VMM_WRITABLE | VMM_NX);

	if (TRAMP_SIZE > PAGE_SIZE) {
		error("SMP trampoline is >4096 bytes large.\n");
	}

	uint8_t *virt_base = (uint8_t *)TRAMP_BASE_ADDR;
	memcpy(virt_base, smp_tramp_start, TRAMP_SIZE);
	memset(&virt_base[TRAMP_DATA_OFF], 0, PAGE_SIZE - TRAMP_DATA_OFF);

	*((uint64_t *)&virt_base[TRAMP_PML4]) = (uint64_t)kernel_pm;
	*((uint64_t *)&virt_base[TRAMP_ENTRY]) = (uint64_t)smp_cpu_startup;

	atomic_init(&cpu_ready, false);

	info("SMP trampoline initialized, starting up other cores...\n");

	// wake up! grab a brush and put a little makeup!
	for (size_t i = 0; i < lapic_count; i++) {
		if (lapics[i]->id == bsp_id) {
			continue;
		}

		send_start_ipi(i, lapics[i]->id);

		int retries = 0;
		while (retries++ <= CORE_STARTUP_MAX_RETRIES) {
			if (atomic_load(&cpu_ready)) {
				success("CPU %u is up and running\n", i);
				break;
			}
			hpet_msleep(10);
		}

		if (!atomic_load(&cpu_ready)) {
			error("Core %u didn't start up in time, skipping.\n", i);
		}

		atomic_store(&cpu_ready, false);
	}

	// remove trampoline
	unmap_page(NULL, (uintptr_t)tramp_stack);
	unmap_page(NULL, TRAMP_BASE_ADDR);
	pfree(tramp_stack, 1);
	tramp_stack = NULL;

	cpu_enable_interrupts();
	smp_initialized = true;
}

__attribute__((noreturn)) void smp_cpu_startup(uint8_t cpu)
{
	cpu_early_init();
	cpu_init();

	// set up own stack
	void *stack = palloc(4); // 16kib
	if (!stack) {
		error("Couldn't allocate stack for CPU %u, halting execution.\n", cpu);
		cpu_halt();
	}

	map_pages(NULL, (uintptr_t)stack, (uintptr_t)stack, 16 * 1024,
			  VMM_PRESENT | VMM_WRITABLE | VMM_NX);

	__asm__ volatile("mov %0, %%rsp" ::"r"(stack + (16 * 1024)));

	cpu_enable_interrupts();

	// we rollin' in parallel now
	atomic_store(&cpu_ready, true);

	debug("cpu%u: s=0x%llx, l=%u\n", cpu, stack, 16 * 1024);

	// sched_init();
	for (;;) {
		__asm__ volatile("hlt");
	}
	UNREACHABLE();
}