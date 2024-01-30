%include "Arches/x86_64/Build.asm"

ExternLabel g_IDT

GlobalLabel IDTLoad ; void IDTLoad(void)
    sub rsp, 10h
    mov word [rsp], 4095
    mov qword [rsp +2], g_IDT
    lidt [rsp]
    add rsp, 10h
    ret