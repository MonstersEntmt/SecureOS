%include "x86_64/Build.asminc"

ExternLabel g_x86_64IDT

GlobalLabel x86_64LoadIDT; void x86_64LoadIDT(void)
    sub rsp, 10h
    mov word [rsp], 4095
    mov qword [rsp + 2], g_x86_64IDT
    lidt [rsp]
    add rsp, 10h
    ret