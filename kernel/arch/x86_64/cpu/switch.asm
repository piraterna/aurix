section .text
global switch_task

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

    mov [rdi + KTHREAD_RSP_OFFSET], rsp

.load_next:
    mov rsp, [rsi + KTHREAD_RSP_OFFSET]

    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ret
