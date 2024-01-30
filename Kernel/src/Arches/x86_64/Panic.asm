%include "Arches/x86_64/Build.asm"

GlobalLabel CPUDisableInterrupts ; void CPUDisableInterrupts(void);
    cli
    ret
    
GlobalLabel CPUEnableInterrupts ; void CPUEnableInterrupts(void);
    sti
    ret

GlobalLabel CPUHalt ; void CPUHalt(void)
    .loop:
        cli
        hlt
        jmp .loop
    ret