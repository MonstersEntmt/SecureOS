%include "Arches/x86_64/Build.asm"

GlobalLabel CPUHalt ; void CPUHalt(void)
    .loop:
        cli
        hlt
        jmp .loop
    ret