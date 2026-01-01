/*********************************************************************************/
/* Module Name:  init.c */
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
#include <arch/cpu/gdt.h>
#include <arch/cpu/idt.h>
#include <config.h>
#include <aurix.h>
#include <string.h>
#include <stddef.h>

#define CPU_ID_MSR 0xC0000103

struct cpu cpuinfo[CONFIG_CPU_MAX_COUNT];
size_t cpu_count = 0;

int cpu_early_init()
{
	gdt_init();
	idt_init();

	// save cpuinfo
	cpuinfo[cpu_count].id = cpu_count;
	wrmsr(CPU_ID_MSR, cpu_count);
	cpu_count++;

	return 1; // all good
}

void cpu_init()
{
	struct cpu *cpu = cpu_get_current();
	if (!cpu) {
		error("Couldn't figure out which core I'm running on :/\n");
		return;
	}

	// overwrite default assigned ID with a proper LAPIC ID
	cpu->id = lapic_read(0x20);
	wrmsr(CPU_ID_MSR, cpu->id);

	memset(cpu, 0, sizeof(struct cpu));

	uint32_t extfunc;
	cpuid(0, &extfunc, (uint32_t *)(cpu->vendor_str),
		  (uint32_t *)(cpu->vendor_str + 8), (uint32_t *)(cpu->vendor_str + 4));
	cpu->vendor_str[12] = 0;

	uint32_t eax, ebx, ecx, edx;
	cpuid(0x80000000, &extfunc, &ebx, &ecx, &edx);
	(void)eax;
	if (extfunc >= 0x80000004) {
		cpuid(0x80000002, (uint32_t *)(cpu->name_ext),
			  (uint32_t *)(cpu->name_ext + 4), (uint32_t *)(cpu->name_ext + 8),
			  (uint32_t *)(cpu->name_ext + 12));
		cpuid(0x80000003, (uint32_t *)(cpu->name_ext + 16),
			  (uint32_t *)(cpu->name_ext + 20),
			  (uint32_t *)(cpu->name_ext + 24),
			  (uint32_t *)(cpu->name_ext + 28));
		cpuid(0x80000004, (uint32_t *)(cpu->name_ext + 32),
			  (uint32_t *)(cpu->name_ext + 36),
			  (uint32_t *)(cpu->name_ext + 40),
			  (uint32_t *)(cpu->name_ext + 44));

		int lead = 0;
		char *p = &cpu->name_ext[0];
		while (*p++ == ' ') {
			lead++;
		}

		if (lead >= 1) {
			memcpy(&cpu->name_ext[0], &cpu->name_ext[lead], 48 - lead);
			memset(&cpu->name_ext[48 - lead], 0, lead);
		}
	}
}

struct cpu *cpu_get_current()
{
	uint32_t id = rdmsr(CPU_ID_MSR);
	struct cpu *cpu = &cpuinfo[id];
	if (cpu->id != id) {
		error("I'm running on an unregistered core!\n");
		return NULL;
	}

	return cpu;
}