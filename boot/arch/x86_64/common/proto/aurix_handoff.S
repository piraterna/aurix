.globl _aurix_handoff_start
.globl _aurix_handoff_end
.globl aurix_handoff

aurix_handoff:
	cli
	movq %rsi, %rsp
_aurix_handoff_start:
	movq %rdi, %cr3
	jmpq *%rdx
_aurix_handoff_end: