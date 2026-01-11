[section .text]

global switch_task
switch_task:
    cli

    ; RDI = new RSP
    ; RSI = new CR3 (physical address)

    mov     cr3, rsi
    mov     rsp, rdi

    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    iretq
