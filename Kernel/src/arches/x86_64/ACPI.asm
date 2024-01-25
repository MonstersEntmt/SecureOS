%include "x86_64/Build.asminc"

GlobalLabel GetProcessorID ; uint8_t GetProcessorID(void)
    push rbx
    mov eax, 1
    cpuid
    shr rbx, 24
    and rbx, 0xFF
    mov rax, rbx
    pop rbx
    ret