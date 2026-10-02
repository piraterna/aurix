/*********************************************************************************/
/* Module Name:  cpu.h */
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
#include <arch/sys/irqlock.h>
#include <sys/spinlock.h>
#include <aurix.h>
#include <stdint.h>
#include <stddef.h>

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

enum {
    CPUID_ECX_SSE3 = 1 << 0,
    CPUID_ECX_PCLMUL = 1 << 1,
    CPUID_ECX_DTES64 = 1 << 2,
    CPUID_ECX_MONITOR = 1 << 3,
    CPUID_ECX_DS_CPL = 1 << 4,
    CPUID_ECX_VMX = 1 << 5,
    CPUID_ECX_SMX = 1 << 6,
    CPUID_ECX_EST = 1 << 7,
    CPUID_ECX_TM2 = 1 << 8,
    CPUID_ECX_SSSE3 = 1 << 9,
    CPUID_ECX_CID = 1 << 10,
    CPUID_ECX_SDBG = 1 << 11,
    CPUID_ECX_FMA = 1 << 12,
    CPUID_ECX_CX16 = 1 << 13,
    CPUID_ECX_XTPR = 1 << 14,
    CPUID_ECX_PDCM = 1 << 15,
    CPUID_ECX_PCID = 1 << 17,
    CPUID_ECX_DCA = 1 << 18,
    CPUID_ECX_SSE41 = 1 << 19,
    CPUID_ECX_SSE42 = 1 << 20,
    CPUID_ECX_X2APIC = 1 << 21,
    CPUID_ECX_MOVBE = 1 << 22,
    CPUID_ECX_POPCNT = 1 << 23,
    CPUID_ECX_TSC = 1 << 24,
    CPUID_ECX_AES = 1 << 25,
    CPUID_ECX_XSAVE = 1 << 26,
    CPUID_ECX_OSXSAVE = 1 << 27,
    CPUID_ECX_AVX = 1 << 28,
    CPUID_ECX_F16C = 1 << 29,
    CPUID_ECX_RDRAND = 1 << 30,
    CPUID_ECX_HYPERVISOR = 1 << 31,

    CPUID_EDX_FPU = 1 << 0,
    CPUID_EDX_VME = 1 << 1,
    CPUID_EDX_DE = 1 << 2,
    CPUID_EDX_PSE = 1 << 3,
    CPUID_EDX_TSC = 1 << 4,
    CPUID_EDX_MSR = 1 << 5,
    CPUID_EDX_PAE = 1 << 6,
    CPUID_EDX_MCE = 1 << 7,
    CPUID_EDX_CX8 = 1 << 8,
    CPUID_EDX_APIC = 1 << 9,
    CPUID_EDX_SEP = 1 << 11,
    CPUID_EDX_MTRR = 1 << 12,
    CPUID_EDX_PGE = 1 << 13,
    CPUID_EDX_MCA = 1 << 14,
    CPUID_EDX_CMOV = 1 << 15,
    CPUID_EDX_PAT = 1 << 16,
    CPUID_EDX_PSE36 = 1 << 17,
    CPUID_EDX_PSN = 1 << 18,
    CPUID_EDX_CLFLUSH = 1 << 19,
    CPUID_EDX_DS = 1 << 21,
    CPUID_EDX_ACPI = 1 << 22,
    CPUID_EDX_MMX = 1 << 23,
    CPUID_EDX_FXSR = 1 << 24,
    CPUID_EDX_SSE = 1 << 25,
    CPUID_EDX_SSE2 = 1 << 26,
    CPUID_EDX_SS = 1 << 27,
    CPUID_EDX_HTT = 1 << 28,
    CPUID_EDX_TM = 1 << 29,
    CPUID_EDX_IA64 = 1 << 30,
    CPUID_EDX_PBE = 1 << 31
};

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
			uint8_t sdbg : 1;
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
			uint8_t hypervisor : 1;
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

struct tcb; // forward declare becuz stupid errors with including sched.h
struct cpu {
	uint32_t id;

	struct cpuid cpuid;

	char vendor_str[13];
	char name_ext[48];

	struct tcb *thread_list;
	uint64_t thread_count;

	irqlock_t sched_lock;
};

extern struct cpu cpuinfo[];
extern size_t cpu_count;

struct cpu *cpu_get_current(void);
uint8_t cpu_get_current_id(void);

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

static inline void cpuid(uint32_t leaf, uint32_t subleaf, 
						uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	__asm__ volatile("cpuid"
					 : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
					 : "a"(leaf), "c"(subleaf));
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

static inline uint16_t inw(uint16_t port)
{
	uint16_t ret;
	__asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port) : "memory");
	return ret;
}

static inline void outw(uint16_t port, uint16_t val)
{
	__asm__ volatile("outb %b0, %w1" ::"a"(val), "Nd"(port) : "memory");
}

static inline uint32_t indw(uint16_t port)
{
	uint32_t ret;
	__asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port) : "memory");
	return ret;
}

static inline void outdw(uint16_t port, uint32_t val)
{
	__asm__ volatile("outb %b0, %w1" ::"a"(val), "Nd"(port) : "memory");
}

static inline void io_wait(void)
{
	__asm__ volatile("outb %b0, %w1" : : "a"(0), "Nd"(0x80));
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
