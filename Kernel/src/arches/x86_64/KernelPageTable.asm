%include "x86_64/Build.asminc"

GlobalLabel x86_64KPTSetCR3 ; void x86_64KPTSetCR3(uint64_t cr3, bool isLvl5)
    mov rax, cr4
    cmp esi, 0
    jle .DisableLvl5
    or rax, 0x1000
    jle .SetCR4
.DisableLvl5:
    mov rcx, 0x1000
    not rcx
    and rax, rcx
.SetCR4:
    mov cr4, rax
    mov cr3, rdi
    ret