global memset
global memcpy
global memcpy_reverse
memset: ; void* memset(void* dst, int c, size_t count)
    mov r8, rdi
    mov rcx, rdx
    mov ax, si
    cld
    rep stosb
    mov rax, r8
    ret

memcpy: ; void* memcpy(void* dst, const void* src, size_t count)
    mov r8, rdi
    mov rcx, rdx
    cld
    rep movsb
    mov rax, r8
    ret

memcpy_reverse: ; void* memcpy_reverse(void* dst, const void* src, size_t count)
    mov r8, rdi
    mov rcx, rdx
    add rsi, rcx
    add rdi, rcx
    std
    rep movsb
    mov rax, r8
    ret