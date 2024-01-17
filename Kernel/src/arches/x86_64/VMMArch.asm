%include "x86_64/Build.asminc"

GlobalLabel VMMArchActivate ; void VMMArchActivate(uint64_t* pageTableRoot, uint8_t levels, bool use1GiB)
    mov rax, cr4
    cmp esi, 5
    jl .DisableLvl5
    or rax, 0x1000
    jmp .SetCR4
.DisableLvl5:
    mov rcx, 0x1000
    not rcx
    and rax, rcx
.SetCR4:
    mov cr4, rax
    mov rax, 0x000FFFFFFFFFF000
    and rdi, rax
    mov cr3, rdi
    ret