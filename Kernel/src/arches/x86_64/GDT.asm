%include "x86_64/Build.asminc"

ExternLabel g_x86_64GDT
ExternLabel g_x86_64GDTR

GlobalLabel x86_64LoadGDT ; void x86_64LoadGDT(uint16_t initalCS, uint64_t initialDS)
    mov word [g_x86_64GDTR], 4095
    mov qword [g_x86_64GDTR + 2], g_x86_64GDT
    lgdt [g_x86_64GDTR]
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