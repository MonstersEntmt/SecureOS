%include "x86_64/Build.asminc"

ExternLabel g_x86_64IDT
ExternLabel g_x86_64IDTR

GlobalLabel x86_64LoadIDT; void x86_64LoadIDT(void)
    mov word [g_x86_64IDTR], 4095
    mov qword [g_x86_64IDTR + 2], g_x86_64IDT
    lidt [g_x86_64IDTR]
    ret