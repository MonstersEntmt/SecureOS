%include "x86_64/Build.asminc"

GlobalLabel x86_64FeatureEnable ; bool x86_64FeatureEnable(void)
    mov eax, 0x80000001
    cpuid
    shr edx, 20
    bt edx, 0
    jc .Valid
    mov rax, 0
    ret
    
.Valid:
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x800
    wrmsr

    mov rax, 1
    ret