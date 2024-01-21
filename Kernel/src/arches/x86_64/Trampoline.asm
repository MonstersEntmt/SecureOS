%include "x86_64/Build.asminc"
default abs

%define ADDR_OF(x) (0x1000 + (x - x86_64Trampoline))

bits 16
GlobalLabel x86_64Trampoline ; void x86_64Trampoline(void)
    cli
    jmp 0:ADDR_OF(.InSegment0)
.InSegment0:
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov byte [ADDR_OF(g_x86_64TrampolineStats.Alive)], 1

    .waitForAck:
        cmp byte [ADDR_OF(g_x86_64TrampolineStats.Ack)], 1
        jne .waitForAck
    
    mov eax, cr4
    or eax, 0x20

    mov ebx, [ADDR_OF(g_x86_64TrampolineSettings.PageTableSettings)]
    bt ebx, 0
    jnc .DisableLvl5
    .EnableLvl5:
        or eax, 0x1000
        jmp .SetCR4
    .DisableLvl5:
        mov ecx, 0x1000
        not ecx
        and eax, ecx
    .SetCR4:
        mov cr4, eax
    
    mov eax, [ADDR_OF(g_x86_64TrampolineSettings.PageTable)]
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x900
    wrmsr

    mov eax, cr0
    or eax, 0x80000001
    mov cr0, eax

    lgdt [ADDR_OF(GDTR)]

    jmp GDT.code_64:ADDR_OF(.LongMode)

bits 64
.LongMode:
    mov eax, 1
    cpuid
    shr rbx, 24
    
    mov ax, GDT.data_64
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, ADDR_OF(Stack.top)
    mov rbp, rsp

    mov rcx, [ADDR_OF(g_x86_64TrampolineSettings.StackAllocFn)]
    call rcx
    
    mov rdi, rbx
    mov rsp, rax
    mov rbp, rsp

    mov rcx, [ADDR_OF(g_x86_64TrampolineSettings.CPUTrampolineFn)]
    jmp rcx

; ---------
;  Structs
; ---------

align 16
LocalLabel GDTR
    dw GDT.size
    dd ADDR_OF(GDT)

align 16
LocalLabel GDT
    dq 0
.data_64 equ $ - GDT
    dq 0x00AF93000000FFFF
.code_64 equ $ - GDT
    dq 0x00AF9B000000FFFF
.size equ $ - GDT - 1

align 16
GlobalLabel g_x86_64TrampolineSettings ; struct x86_64TrampolineSettings
    .PageTable:         dq 0
    .PageTableSettings: dq 0
    .CPUTrampolineFn:   dq 0
    .StackAllocFn:      dq 0

align 16
GlobalLabel g_x86_64TrampolineStats ; struct x86_64TrampolineStats
    .Alive: db 0
    .Ack:   db 0

align 16
LocalLabel Stack
    .bottom:
    times 4096 - ($ - x86_64Trampoline) db 0
    .top: