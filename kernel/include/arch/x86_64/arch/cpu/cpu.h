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

#define ECX_SSE3 (1 << 0)
#define ECX_PCLMULQDQ (1 << 1)
#define ECX_DTES64 (1 << 2)
#define ECX_MONITOR (1 << 3)
#define ECX_DS_CPL (1 << 4)
#define ECX_VMX (1 << 5)
#define ECX_SMX (1 << 6)
#define ECX_EST (1 << 7)
#define ECX_TM2 (1 << 8)
#define ECX_SSSE3 (1 << 9)
#define ECX_CNXT_ID (1 << 10)
#define ECX_FMA (1 << 12)
#define ECX_CX16 (1 << 13)
#define ECX_XTPR (1 << 14)
#define ECX_PDCM (1 << 15)
#define ECX_PCID (1 << 17)
#define ECX_DCA (1 << 18)
#define ECX_SSE41 (1 << 19)
#define ECX_SSE42 (1 << 20)
#define ECX_X2APIC (1 << 21)
#define ECX_MOVBE (1 << 22)
#define ECX_POPCNT (1 << 23)
#define ECX_TSC (1 << 24)
#define ECX_AESNI (1 << 25)
#define ECX_XSAVE (1 << 26)
#define ECX_OSXSAVE (1 << 27)
#define ECX_AVX (1 << 28)
#define ECX_F16C (1 << 29)
#define ECX_RDRAND (1 << 30)

#define EDX_FPU (1 << 0)
#define EDX_VME (1 << 1)
#define EDX_DE (1 << 2)
#define EDX_PSE (1 << 3)
#define EDX_TSC (1 << 4)
#define EDX_MSR (1 << 5)
#define EDX_PAE (1 << 6)
#define EDX_MCE (1 << 7)
#define EDX_CX8 (1 << 8)
#define EDX_APIC (1 << 9)
#define EDX_SEP (1 << 11)
#define EDX_MTRR (1 << 12)
#define EDX_PGE (1 << 13)
#define EDX_MCA (1 << 14)
#define EDX_CMOV (1 << 15)
#define EDX_PAT (1 << 16)
#define EDX_PSE36 (1 << 17)
#define EDX_PSN (1 << 18)
#define EDX_CLFLUSH (1 << 19)
#define EDX_DS (1 << 21)
#define EDX_ACPI (1 << 22)
#define EDX_MMX (1 << 23)
#define EDX_FXSR (1 << 24)
#define EDX_SSE (1 << 25)
#define EDX_SSE2 (1 << 26)
#define EDX_SS (1 << 27)
#define EDX_HTT (1 << 28)
#define EDX_TM (1 << 29)
#define EDX_PBE (1 << 31)

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