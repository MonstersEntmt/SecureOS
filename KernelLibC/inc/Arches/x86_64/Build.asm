%ifndef ARCHES_X86_64_BUILD_ASM
%define ARCHES_X86_64_BUILD_ASM

default rel

%define BUILD_CONFIG_DEBUG   1
%define BUILD_CONFIG_DIST    2
%define BUILD_CONFIG_RELEASE (BUILD_CONFIG_DEBUG | BUILD_CONFIG_DIST)

%define BUILD_IS_CONFIG_DEBUG   (BUILD_CONFIG & BUILD_CONFIG_DEBUG)
%define BUILD_IS_CONFIG_RELEASE (BUILD_CONFIG == BUILD_CONFIG_RELEASE)
%define BUILD_IS_CONFIG_DIST    (BUILD_CONFIG & BUILD_CONFIG_DIST)

%macro ExternLabel 1
    extern %1
%endmacro
%macro GlobalLabel 1
    global %1
    %1:
%endmacro
%macro LocalLabel 1
    static %1
    %1:
%endmacro

%endif