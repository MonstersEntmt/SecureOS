default rel

extern g_GDT
extern g_IDT

align 16
    dw 0, 0, 0
gdtr_limit:
idtr_limit:
    dw 0
gdtr_addr:
idtr_addr:
    dq 0

global LoadDescriptorTables
LoadDescriptorTables: ; void LoadDescriptorTables(void)
    lea rax, [g_GDT]
    mov word [gdtr_limit], 4096
    mov qword [gdtr_addr], rax
    lgdt [gdtr_limit]

    lea rax, [g_IDT]
    mov word [idtr_limit], 4096
    mov qword [idtr_addr], rax
    lidt [idtr_limit]
    ret

extern DefaultInterruptHandler
global DefaultInterruptHandlerWrapper
DefaultInterruptHandlerWrapper: ; void DefaultInterruptHandler(struct InterruptHandlerData* data)
    ;cld
    ;mov rdi, rsp
    ;call DefaultInterruptHandler
    iretq