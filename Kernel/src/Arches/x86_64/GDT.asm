%include "Arches/x86_64/Build.asm"

ExternLabel g_GDT

GlobalLabel GDTLoad ; void GDTLoad(uint16_t initialCS, uint16_t initialDS)
    ; di => uint16_t initialCS
    ; si => uint16_t initialDS
    sub rsp, 10h
    lea rax, [g_GDT]
    mov word [rsp], 4095
    mov qword [rsp + 2], rax
    lgdt [rsp]
    add rsp, 10h

    push rdi
    lea rax, [.reloadCS]
    push rax
    retfq
    .reloadCS:
        mov ax, si
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax
        ret

GlobalLabel LDTLoad ; void LDTLoad(uint16_t descriptor)
    ; di => uint16_t descriptor
    lldt di
    ret