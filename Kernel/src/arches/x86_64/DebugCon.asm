%include "x86_64/Build.asminc"

GlobalLabel DebugCon_WriteChar ; void DebugCon_WriteChar(char c)
%if BUILD_IS_CONFIG_DEBUG
    mov dx, 0xE9
    mov ax, di
    out dx, al
%endif
    ret

GlobalLabel DebugCon_WriteChars ; void DebugCon_WriteChars(const char* str, size_t count)
%if BUILD_IS_CONFIG_DEBUG
    mov dx, 0xE9
    mov rcx, rsi
    mov rsi, rdi
    cld
    rep outsb
%endif
    ret