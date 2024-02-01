%include "Arches/x86_64/Build.asm"

ExternLabel VMMGetRootTable ; void* VMMGetRootTable(void* vmm, uint8_t* levels, bool* use1GiB)
    ; rax => void* rootTable
    ; rdi => void* vmm
    ; rsi => uint8_t* levels
    ; rdx => bool* use1GiB

GlobalLabel VMMActivate ; void VMMActivate(void* vmm)
    ; rdi => void* vmm
    sub rsp, 10h
    ; rsp + 16 => return RIP
    ; rsp      => uint8_t levels

    lea rsi, [rsp]
    xor rdx, rdx
    call VMMGetRootTable
    mov rdi, rax
    
    mov rax, cr4

    movzx ecx, byte[rsp]
    cmp ecx, 5
    jge .EnableLvl5
    .DisableLvl5:
        mov rcx, 0x1000
        not rcx
        and rax, rcx
        jmp .SetCR4
    .EnableLvl5:
        or rax, 0x1000
    .SetCR4:
        mov cr4, rax

    mov rax, 0x000FFFFFFFFFF000
    and rdi, rax
    mov cr3, rdi

    add rsp, 10h
    ret