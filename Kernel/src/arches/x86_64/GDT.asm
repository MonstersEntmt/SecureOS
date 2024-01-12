%include "x86_64/Build.asminc"

ExternLabel g_x86_64GDT

GlobalLabel x86_64LoadGDT ; void x86_64LoadGDT(uint16_t initalCS, uint64_t initialDS)
    sub rsp, 10h
    mov word [rsp], 4095
    mov qword [rsp + 2], g_x86_64GDT
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

GlobalLabel x86_64LoadLDT ; void x86_64LoadLDT(uint16_t segment)
    lldt di
    ret