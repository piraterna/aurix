/*********************************************************************************/
/* Module Name:  cpu.h */
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

#ifndef _ARCH_CPU_CPU_H
#define _ARCH_CPU_CPU_H

#include <arch/cpu/smp.h>
#include <aurix.h>
#include <stdint.h>

struct interrupt_frame {
	uint64_t es;
	uint64_t ds;

	uint64_t cr0;
	uint64_t cr2;
	uint64_t cr3;
	uint64_t cr4;
	//uint64_t cr8;

	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rbp;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;

	uint64_t vector;
	uint64_t err;

	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} __attribute__((packed));

struct stack_frame {
	struct stack_frame *rbp;
	uint64_t rip;
} __attribute__((packed));

struct cpuid {
	union {
		uint32_t ecx;
		struct {
			uint8_t sse3 : 1;
			uint8_t pclmulqdq : 1;
			uint8_t dtes64 : 1;
			uint8_t monitor : 1;
			uint8_t ds_cpl : 1;
			uint8_t vmx : 1;
			uint8_t smx : 1;
			uint8_t est : 1;
			uint8_t tm2 : 1;
			uint8_t ssse3 : 1;
			uint8_t cnxt_id : 1;
			uint8_t fma : 1;
			uint8_t cx16 : 1;
			uint8_t xtpr : 1;
			uint8_t pdcm : 1;
			uint8_t pcid : 1;
			uint8_t dca : 1;
			uint8_t sse41 : 1;
			uint8_t sse42 : 1;
			uint8_t x2apic : 1;
			uint8_t movbe : 1;
			uint8_t popcnt : 1;
			uint8_t tsc : 1;
			uint8_t aesni : 1;
			uint8_t xsave : 1;
			uint8_t osxsave : 1;
			uint8_t avx : 1;
			uint8_t f16c : 1;
			uint8_t rdrand : 1;
		} __attribute__((packed)) ecx_bits;
	};
	union {
		uint32_t edx;
		struct {
			uint8_t fpu : 1;
			uint8_t vme : 1;
			uint8_t de : 1;
			uint8_t pse : 1;
			uint8_t tsc : 1;
			uint8_t msr : 1;
			uint8_t pae : 1;
			uint8_t mce : 1;
			uint8_t cx8 : 1;
			uint8_t apic : 1;
			uint8_t sep : 1;
			uint8_t mtrr : 1;
			uint8_t pge : 1;
			uint8_t mca : 1;
			uint8_t cmov : 1;
			uint8_t pat : 1;
			uint8_t pse36 : 1;
			uint8_t psn : 1;
			uint8_t clflush : 1;
			uint8_t ds : 1;
			uint8_t acpi : 1;
			uint8_t mmx : 1;
			uint8_t fxsr : 1;
			uint8_t sse : 1;
			uint8_t sse2 : 1;
			uint8_t ss : 1;
			uint8_t htt : 1;
			uint8_t tm : 1;
			uint8_t pbe : 1;
		} __attribute__((packed)) edx_bits;
	};
};

struct cpu {
	uint32_t id;

	struct cpuid cpuid;
	char vendor_str[13];
	char name_ext[48];
};

struct cpu *cpu_get_current(void);

////
// Utilities
///

static inline void cpu_init_mp(void)
{
	smp_init();
}

static inline void cpu_nop(void)
{
	__asm__ volatile("nop");
}

static inline void cpu_halt(void)
{
	for (;;) {
		__asm__ volatile("cli;hlt");
	}

	UNREACHABLE();
}

static inline void cpu_enable_interrupts(void)
{
	__asm__ volatile("sti");
}

static inline void cpu_disable_interrupts(void)
{
	__asm__ volatile("cli");
}

static inline void cpuid(uint32_t reg, uint32_t *eax, uint32_t *ebx,
						 uint32_t *ecx, uint32_t *edx)
{
	__asm__ volatile("cpuid"
					 : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
					 : "0"(reg)
					 : "memory");
}

static inline uint64_t read_cr0()
{
	uint64_t val;
	__asm__ volatile("mov %%cr0, %0" : "=r"(val));
	return val;
}

static inline uint64_t read_cr2()
{
	uint64_t val;
	__asm__ volatile("mov %%cr2, %0" : "=r"(val));
	return val;
}

static inline uint64_t read_cr3()
{
	uint64_t val;
	__asm__ volatile("mov %%cr3, %0" : "=r"(val));
	return val;
}

static inline uint64_t read_cr4()
{
	uint64_t val;
	__asm__ volatile("mov %%cr4, %0" : "=r"(val));
	return val;
}

static inline uint64_t read_cr8()
{
	uint64_t val;
	__asm__ volatile("mov %%cr8, %0" : "=r"(val));
	return val;
}

static inline void write_cr0(uint64_t val)
{
	__asm__ volatile("mov %0, %%cr0" ::"r"(val));
}

static inline void write_cr2(uint64_t val)
{
	__asm__ volatile("mov %0, %%cr2" ::"r"(val));
}

static inline void write_cr3(uint64_t val)
{
	__asm__ volatile("mov %0, %%cr3" ::"r"(val) : "memory");
}

static inline void write_cr4(uint64_t val)
{
	__asm__ volatile("mov %0, %%cr4" ::"r"(val));
}

static inline void write_cr8(uint64_t val)
{
	__asm__ volatile("mov %0, %%cr8" ::"r"(val));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t ret;
	__asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port) : "memory");
	return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
	__asm__ volatile("outb %b0, %w1" ::"a"(val), "Nd"(port) : "memory");
}

static inline void invlpg(void *addr)
{
	__asm__ volatile("invlpg (%0)" ::"b"(addr) : "memory");
}

static inline void wrmsr(uint64_t msr, uint64_t val)
{
	uint32_t lo = val & 0xFFFFFFFF;
	uint32_t hi = val >> 32;
	__asm__ volatile("wrmsr" ::"c"(msr), "a"(lo), "d"(hi));
}

static inline uint64_t rdmsr(uint64_t msr)
{
	uint32_t lo, hi;
	__asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
	return ((uint64_t)hi << 32) | lo;
}

////
// Spinlock util
////
static inline void cpu_spinwait(void)
{
	__asm__ volatile("pause" ::: "memory");
}

#endif /* _ARCH_CPU_CPU_H */