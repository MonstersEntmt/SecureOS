%include "Arches/x86_64/Build.asm"

GlobalLabel PagingGetCR2 ; void* PagingGetCR2(void)
    mov rax, cr2
    ret