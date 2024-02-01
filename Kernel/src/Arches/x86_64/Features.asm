%include "Arches/x86_64/Build.asm"

ExternLabel puts ; void puts(const char* str)
    ; rdi => const char* str

ExternLabel KernelPanic ; void KernelPanic(void)

GlobalLabel ArchEnableNXE ; void ArchEnableNXE(void)
    push rbx
    mov eax, 0x80000001
    cpuid
    shr edx, 20
    bt edx, 0
    jc .Valid
    .Invalid:
        lea rdi, [ArchNXENotAvailableMsg]
        call puts
        call KernelPanic

    .Valid:
        mov ecx, 0xC0000000
        rdmsr
        or eax, 0x800
        wrmsr
        
        pop rbx
        ret

LocalLabel ArchNXENotAvailableMsg
    db "CRITICAL: Requires NXE support on x86-64", 0