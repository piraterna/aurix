[section .text]

%include "arch/asm/macros.inc"

[extern isr_common_handler]
isr_handler_stub:
	cli
	pushaq

	mov rax, cr4
	push rax
	mov rax, cr3
	push rax
	mov rax, cr2
	push rax
	mov rax, cr0
	push rax

	mov rax, ds
	push rax
	mov rax, es
	push rax

	cld
	call isr_common_handler

	add rsp, 0x30
	popaq
	add rsp, 0x0F

	sti
	iretq

%macro create_isr 1
isr_%1:
%if %1 != 8 && %1 != 10 && %1 != 11 && %1 != 12 && %1 != 13 && %1 != 14 && %1 != 17 && %1 != 30
	push 0
%endif
	push %1
	jmp isr_handler_stub
	ret
%endmacro

%assign i 0
%rep 256
create_isr i
%assign i i+1
%endrep

[section .data]

[global isr_stubs]
isr_stubs:
%assign i 0
%rep 256
	dq isr_%+i
%assign i i+1
%endrep