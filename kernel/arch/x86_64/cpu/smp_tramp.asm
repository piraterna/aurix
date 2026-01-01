; This file is a modified version of PatchworkOS's SMP trampoline code.
;; https://github.com/KaiNorberg/PatchworkOS | MIT

%define TRAMP_PHYS(addr) (addr - smp_tramp_start + TRAMP_BASE)

%define TRAMP_ADDR(addr) (TRAMP_BASE + (addr))

TRAMP_BASE equ 0x8000
TRAMP_DATA equ 0xF00
TRAMP_PML4 equ (TRAMP_DATA)
TRAMP_ENTRY equ (TRAMP_DATA + 0x08)
TRAMP_CPU equ (TRAMP_DATA + 0x10)
TRAMP_STACK equ (TRAMP_DATA + 0x18)

global smp_tramp_start:function
global smp_tramp_end:function

section .text
bits 16
align 16

smp_tramp_start:
	cli
	cld

	xor ax, ax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	o32 lgdt [TRAMP_PHYS(gdtr32)]

	xor eax, eax
	or eax, 1 ; PE
	mov cr0, eax
	jmp 0x08:(TRAMP_PHYS(smp_tramp_prot))

bits 32
smp_tramp_prot:
	mov bx, 0x10
	mov ds, bx
	mov es, bx
	mov ss, bx

	mov eax, cr4
	or eax, (1 << 5) ; PAE
	mov cr4, eax

	mov eax, [TRAMP_ADDR(TRAMP_PML4)]
	mov cr3, eax

	mov ecx, 0xC0000080 ; MSR_EFER
	rdmsr
	or eax, (1 << 8) ; LME
	or eax, (1 << 11) ; NXE
	wrmsr

	mov eax, cr0
	or eax, (1 << 31) ; PG
	mov cr0, eax

	lgdt [TRAMP_PHYS(gdtr64)]
	jmp 0x08:(TRAMP_PHYS(smp_tramp_long))

default abs
bits 64
smp_tramp_long:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov ss, ax

	xor ax, ax
	mov fs, ax
	mov gs, ax

	mov rsp, [TRAMP_ADDR(TRAMP_STACK)]
	xor rbp, rbp

	push 0x0
	popfq

	mov rdi, [TRAMP_ADDR(TRAMP_CPU)]
	jmp [TRAMP_ADDR(TRAMP_ENTRY)]

align 16
gdtr32:
	dw gdt32_end - gdt32_start - 1
	dd TRAMP_PHYS(gdt32_start)
align 16
gdt32_start:
	dq 0
	dq 0x00CF9A000000FFFF
	dq 0x00CF92000000FFFF
gdt32_end:

align 16
gdtr64:
	dw gdt64_end - gdt64_start - 1
	dq TRAMP_PHYS(gdt64_start)
align 16
gdt64_start:
	dq 0
	dq 0x00AF98000000FFFF
	dq 0x00CF92000000FFFF
gdt64_end:

align 16
smp_tramp_end: