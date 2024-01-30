%include "Arches/x86_64/Build.asm"

%macro InterruptWrapper 1
ExternLabel %1
GlobalLabel %1Wrapper
    lea rdi, [rsp]
    call %1
    iretq
%endmacro

%macro ExceptionWrapper 1
ExternLabel %1
GlobalLabel %1Wrapper
    pop rsi
    lea rdi, [rsp]
    cli
    call %1
    sti
    iretq
%endmacro

InterruptWrapper DEInterruptHandler
InterruptWrapper UDInterruptHandler
ExceptionWrapper DFExceptionHandler
ExceptionWrapper GPExceptionHandler
ExceptionWrapper PFExceptionHandler