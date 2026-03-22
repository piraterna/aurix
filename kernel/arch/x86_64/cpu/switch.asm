section .text
global switch_task
global switch_enter_user

%define KTHREAD_CR3_OFFSET 0
%define KTHREAD_RSP_OFFSET 16
%define KTHREAD_FS_BASE_OFFSET 24

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

    mov ecx, 0xC0000100
    rdmsr
    shl rdx, 32
    or rax, rdx
    mov [rdi + KTHREAD_FS_BASE_OFFSET], rax

.load_next:
    mov rax, [rsi + KTHREAD_CR3_OFFSET]
    mov cr3, rax
    mov rsp, [rsi + KTHREAD_RSP_OFFSET]

    mov rax, [rsi + KTHREAD_FS_BASE_OFFSET]
    mov ecx, 0xC0000100
    mov r8d, eax
    shr rax, 32
    mov edx, eax
    mov eax, r8d
    wrmsr

    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ret

switch_enter_user:
    mov ax, 0x1b
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    iretq
