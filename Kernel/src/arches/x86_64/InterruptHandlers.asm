%include "x86_64/Build.asminc"

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
    call %1
    iretq
%endmacro

ExceptionWrapper x86_64GPExceptionHandler
InterruptWrapper x86_64TestInterruptHandler