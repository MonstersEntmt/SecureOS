global DebugCon_WriteChar
global DebugCon_WriteChars
DebugCon_WriteChar: ; void DebugCon_WriteChar(char c)
    mov dx, 0xE9
    mov ax, di
    out dx, al
    ret

DebugCon_WriteChars: ; void DebugCon_WriteChars(const char* str, size_t count)
    mov dx, 0xE9
    mov rcx, rsi
    mov rsi, rdi
    cld
    rep outsb
    ret