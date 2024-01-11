KERNEL_LIBC_C_SRCS += $(shell find Kernel/clib/src/ -name '*.c' -a -path */arches/x86_64/*)
KERNEL_LIBC_ASM_SRCS += $(shell find Kernel/clib/src/ -name '*.asm' -a -path */arches/x86_64/*)

KERNEL_C_SRCS += $(shell find Kernel/src/ -name '*.c' -a -path */arches/x86_64/*)
KERNEL_ASM_SRCS += $(shell find Kernel/src/ -name '*.asm' -a -path */arches/x86_64/*)

KERNEL_LIBC_CFLAGS += --target=x86_64 -fPIE -DBUILD_ARCH=BUILD_ARCH_X86_64
KERNEL_LIBC_ASMFLAGS += -f elf64 -i Kernel/clib/inc

KERNEL_CFLAGS += --target=x86_64 -fPIE -DBUILD_ARCH=BUILD_ARCH_X86_64
KERNEL_ASMFLAGS += -f elf64 -i Kernel/clib/inc -i Kernel/inc
KERNEL_LDFLAGS +=