%include "x86_64/Build.asminc"

ExternLabel g_x86_64IDT

GlobalLabel x86_64LoadIDT; void x86_64LoadIDT(void)
    sub rsp, 0x10
    mov word [rsp], 511
    mov qword [rsp + 2], g_x86_64IDT
    lidt [rsp]
    add rsp, 0x10
    ret