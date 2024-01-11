%include "x86_64/Build.asminc"

ExternLabel g_x86_64GDT

GlobalLabel x86_64LoadGDT; void x86_64LoadGDT(void)
    sub rsp, 0x10
    mov word [rsp], 511
    mov qword [rsp + 2], g_x86_64GDT
    lgdt [rsp]
    add rsp, 0x10
    ret

GlobalLabel x86_64LoadLDT; void x86_64LoadLDT(uint16_t segment)
    lldt di
    ret