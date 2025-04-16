.globl _aurix_handoff_start
.globl _aurix_handoff_end
.globl aurix_handoff

_aurix_handoff_start:
aurix_handoff:
	cli
	movq %rsi, %rsp
.section _aurix_handoff
	movq %rdi, %cr3
	xor %rax, %rax
	xor %rbx, %rbx
	xor %rcx, %rcx
	xor %rdi, %rdi
	xor %r8, %r8
	xor %r9, %r9
	xor %r10, %r10
	xor %r11, %r11
	xor %r12, %r12
	xor %r13, %r13
	xor %r14, %r14
	xor %r15, %r15
	callq *%rdx
_aurix_handoff_end: