%include "Arches/x86_64/Build.asm"

GlobalLabel memcmp ; int memcmp(const void* lhs, const void* rhs, size_t count)
    ; rdi => const void* lhs
    ; rsi => const void* rhs
    ; rdx => size_t count
    mov rcx, rdx
    cld
    repe cmpsb
    mov rax, 0
    seta al
    mov cl, -1
    cmovb eax, ecx
    movzx rax, al
    ret

GlobalLabel memset ; void* memset(void* dst, int c, size_t count)
    ; rdi => void* dst
    ; esi => int c
    ; rdx => count
    mov r8, rdi
    mov rcx, rdx
    mov ax, si
    cld
    rep stosb
    mov rax, r8
    ret

GlobalLabel memcpy ; void* memcpy(void* dst, const void* src, size_t count)
    ; rdi => void* dst
    ; rsi => const void* src
    ; rdx => size_t count
    mov r8, rdi
    mov rcx, rdx
    cld
    rep movsb
    mov rax, r8
    ret

GlobalLabel memcpy_reverse ; void* memcpy_reverse(void* dst, const void* src, size_t count)
    ; rdi => void* dst
    ; rsi => const void* src
    ; rdx => size_t count
    mov r8, rdi
    mov rcx, rdx
    std
    rep movsb
    mov rax, r8
    ret