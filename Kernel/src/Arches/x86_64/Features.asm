%include "Arches/x86_64/Build.asm"

ExternLabel puts ; void puts(const char* str)
    ; rdi => const char* str

ExternLabel KernelPanic ; void KernelPanic(void)

GlobalLabel ArchEnableNXE ; void ArchEnableNXE(void)
    push rbx
    mov eax, 0x80000001
    cpuid
    bt edx, 20
    jc .Valid
    .Invalid:
        lea rdi, [ArchNXENotAvailableMsg]
        call puts
        call KernelPanic

    .Valid:
        lea rdi, [ArchNXEAvailableMsg]
        call puts

        mov ecx, 0xC0000080
        rdmsr
        or eax, 0x800
        wrmsr
        
        pop rbx
        ret

LocalLabel ArchNXEAvailableMsg
    db "INFO: NXE is supported", 0
LocalLabel ArchNXENotAvailableMsg
    db "CRITICAL: Requires NXE support on x86-64", 0

ExternLabel g_Features  ; struct CPUFeatures
ExternLabel g_XSaveSize ; size_t

%define CPUFeaturesFXSAVEBit 0
%define CPUFeaturesXSAVEBit  1
%define CPUFeaturesSSE1Bit   2
%define CPUFeaturesSSE2Bit   3
%define CPUFeaturesSSE3Bit   4
%define CPUFeaturesSSSE3Bit  5
%define CPUFeaturesSSE4_1Bit 6
%define CPUFeaturesSSE4_2Bit 7
%define CPUFeaturesSSE4ABit  8
%define CPUFeaturesAVXBit    9
%define CPUFeaturesAVX2Bit   10
%define CPUFeaturesAVX512Bit 11
%define CPUFeaturesXOPBit    12
%define CPUFeaturesFMABit    13
%define CPUFeaturesFMA4Bit   14

%define CPUFeaturesFXSAVE (1 << CPUFeaturesFXSAVEBit)
%define CPUFeaturesXSAVE  (1 << CPUFeaturesXSAVEBit)
%define CPUFeaturesSSE1   (1 << CPUFeaturesSSE1Bit)
%define CPUFeaturesSSE2   (1 << CPUFeaturesSSE2Bit)
%define CPUFeaturesSSE3   (1 << CPUFeaturesSSE3Bit)
%define CPUFeaturesSSSE3  (1 << CPUFeaturesSSSE3Bit)
%define CPUFeaturesSSE4_1 (1 << CPUFeaturesSSE4_1Bit)
%define CPUFeaturesSSE4_2 (1 << CPUFeaturesSSE4_2Bit)
%define CPUFeaturesSSE4A  (1 << CPUFeaturesSSE4ABit)
%define CPUFeaturesAVX    (1 << CPUFeaturesAVXBit)
%define CPUFeaturesAVX2   (1 << CPUFeaturesAVX2Bit)
%define CPUFeaturesAVX512 (1 << CPUFeaturesAVX512Bit)
%define CPUFeaturesXOP    (1 << CPUFeaturesXOPBit)
%define CPUFeaturesFMA    (1 << CPUFeaturesFMABit)
%define CPUFeaturesFMA4   (1 << CPUFeaturesFMA4Bit)

GlobalLabel ArchEnableSSE ; void ArchEnableSSE(void)
    push rbx

    ; Enable OSFXSR and OSXMMEXCPT
    mov rax, cr4
    or rax, 0x300
    mov cr4, rax

    ; Enable SSE
    mov rax, cr0
    btr rax, 2
    bts rax, 1
    mov cr0, rax

    mov eax, 1
    cpuid

    ; Check for FXSAVE support
    xor r8, r8
    mov r9, CPUFeaturesFXSAVE
    bt edx, 24
    cmovc r8, r9

    ; Check for XSAVE support
    mov r9, r8
    or r9, CPUFeaturesXSAVE
    bt ecx, 26
    cmovc r8, r9
    
    ; Check for SSE1 support
    mov r9, r8
    or r9, CPUFeaturesSSE1
    bt edx, 25
    cmovc r8, r9
    
    ; Check for SSE2 support
    mov r9, r8
    or r9, CPUFeaturesSSE2
    bt edx, 26
    cmovc r8, r9

    ; Check for SSE3 support
    mov r9, r8
    or r9, CPUFeaturesSSE3
    bt ecx, 0
    cmovc r8, r9
    
    ; Check for SSSE3 support
    mov r9, r8
    or r9, CPUFeaturesSSSE3
    bt ecx, 9
    cmovc r8, r9
    
    ; Check for SSE4_1 support
    mov r9, r8
    or r9, CPUFeaturesSSE4_1
    bt ecx, 19
    cmovc r8, r9
    
    ; Check for SSE4_2 support
    mov r9, r8
    or r9, CPUFeaturesSSE4_2
    bt ecx, 20
    cmovc r8, r9

    mov eax, 0x80000001
    cpuid

    ; Check for SSE4A support
    mov r9, r8
    or r9, CPUFeaturesSSE4A
    bt ecx, 6
    cmovc r8, r9

    bt r8, CPUFeaturesXSAVEBit
    jnc .NoXSave
    .HasXSave:
        mov rax, cr4
        bts rax, 18
        mov cr4, rax

        mov eax, 0x0D
        xor ecx, ecx
        cpuid

        mov qword [g_XSaveSize], rcx

        mov ecx, 0
        xsetbv

        mov eax, 1
        cpuid
    
        ; Check for AVX support
        mov r9, r8
        or r9, CPUFeaturesAVX
        bt ecx, 28
        cmovc r8, r9
        
        ; Check for FMA support
        mov r9, r8
        or r9, CPUFeaturesFMA
        bt ecx, 12
        cmovc r8, r9

        mov eax, 0x80000001
        cpuid

        ; Check for XOP support
        mov r9, r8
        or r9, CPUFeaturesXOP
        bt ecx, 11
        cmovc r8, r9

        ; Check for FMA4 support
        mov r9, r8
        or r9, CPUFeaturesFMA4
        bt ecx, 16
        cmovc r8, r9

        bt r8, CPUFeaturesAVXBit
        jnc .Done
        
        mov eax, 7
        xor ecx, ecx
        cpuid
        
        ; Check for AVX2 support
        mov r9, r8
        or r9, CPUFeaturesAVX2
        bt ebx, 5
        cmovc r8, r9
        
        ; Check for AVX512 support
        mov r9, r8
        or r9, CPUFeaturesAVX512
        bt ebx, 16
        cmovc r8, r9

        jmp .Done

    .NoXSave:
        mov qword [g_XSaveSize], 512

    .Done:
    mov qword [g_Features], r8

    pop rbx
    ret