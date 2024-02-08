KERNEL_C_SRCS   += $(shell find Kernel/src/ -name '*.c' -a -path */Arches/x86_64/*)
KERNEL_ASM_SRCS += $(shell find Kernel/src/ -name '*.asm' -a -path */Arches/x86_64/*)

KERNEL_CFLAGS   += --target=x86_64 -DBUILD_ARCH=BUILD_ARCH_X86_64-mno-mmx -mno-3dnow
KERNEL_ASMFLAGS += -f elf64 -i KernelLibC/inc -i Kernel/inc