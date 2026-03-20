section .text
global x86_64_syscall_entry

extern gdt_tss
extern x86_64_syscall_dispatch

%define CPU_ID_MSR 0xC0000103
%define TSS_SIZE 104
%define TSS_RSP0_OFFSET 4

x86_64_syscall_entry:
    push r11
    push rcx
    push r9
    push r8
    push r10
    push rdx
    push rsi
    push rdi
    push rax

    mov r10, rsp
    lea r9, [rsp + 72]

    mov ecx, CPU_ID_MSR
    rdmsr
    imul rax, TSS_SIZE
    lea rdx, [rel gdt_tss]
    mov rsp, [rdx + rax + TSS_RSP0_OFFSET]

    push r9
    push qword [r10 + 64]
    push qword [r10 + 56]
    push qword [r10 + 48]
    push qword [r10 + 40]
    push qword [r10 + 32]
    push qword [r10 + 24]
    push qword [r10 + 16]
    push qword [r10 + 8]
    push qword [r10 + 0]

    mov rdi, rsp
    call x86_64_syscall_dispatch

    mov rdi, [rsp + 8]
    mov rsi, [rsp + 16]
    mov rdx, [rsp + 24]
    mov r10, [rsp + 32]
    mov r8, [rsp + 40]
    mov r9, [rsp + 48]
    mov rcx, [rsp + 56]
    mov r11, [rsp + 64]
    mov rdx, [rsp + 72]
    add rsp, 80
    mov rsp, rdx
    db 0x48, 0x0f, 0x07
