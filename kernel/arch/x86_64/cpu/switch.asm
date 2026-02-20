section .text
global switch_task

%define KTHREAD_CR3_OFFSET 0
%define KTHREAD_RSP_OFFSET 16

switch_task:
    test rdi, rdi
    jz .load_next

    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    pushfq

    mov rax, cr3
    mov [rdi + KTHREAD_CR3_OFFSET], rax
    mov [rdi + KTHREAD_RSP_OFFSET], rsp

.load_next:
    mov rax, [rsi + KTHREAD_CR3_OFFSET]
    mov cr3, rax
    mov rsp, [rsi + KTHREAD_RSP_OFFSET]

    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ret
