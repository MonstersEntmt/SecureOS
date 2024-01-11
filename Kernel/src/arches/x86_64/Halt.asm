%include "x86_64/Build.asminc"

GlobalLabel DisableInterrupts ; void DisableInterrupts(void)
    cli
    ret

GlobalLabel EnableInterrupts ; void EnableInterrupts(void)
    sti
    ret

GlobalLabel CPUHalt ; void CPUHalt(void)
    cli
    .loop:
        hlt
        jmp .loop
    ret