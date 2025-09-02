.section .text

.globl isr_stubs
.extern isr_common_handler

isr_handler_stub:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %cr0, %rax
    pushq %rax
    movq %cr2, %rax
    pushq %rax
    movq %cr3, %rax
    pushq %rax
    movq %cr4, %rax
    pushq %rax

    movq %ds, %rax
    pushq %rax
    movq %es, %rax
    pushq %rax

    cld

    callq isr_common_handler

    addq $48, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rsi
    popq %rdi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp

    iretq

.macro create_isr index
.global _isr\index
.align 16
_isr\index:
.if \index != 8 && \index != 10 && \index != 11 && \index != 12 && \index != 13 && \index != 14 && \index != 17 && \index != 30
    pushq $0
.endif
    pushq $\index
    jmp isr_handler_stub
.endm

.altmacro

.macro isr_label index
    .section .text
    create_isr \index

    .section .data
    .quad _isr\index
.endm

.section .data
.global isr_stubs
.align 16
isr_stubs:
.set i,0
.rept 256
    isr_label %i
    .set i, i+1
.endr