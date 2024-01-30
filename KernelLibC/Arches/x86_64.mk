KERNEL_LIBC_C_SRCS   += $(shell find KernelLibC/src/ -name '*.c' -a -path */Arches/x86_64/*)
KERNEL_LIBC_ASM_SRCS += $(shell find KernelLibC/src/ -name '*.asm' -a -path */Arches/x86_64/*)

KERNEL_LIBC_CFLAGS   += --target=x86_64 -DBUILD_ARCH=BUILD_ARCH_X86_64 -mno-80387 -mno-mmx -mno-3dnow -mno-sse -mno-sse2
KERNEL_LIBC_ASMFLAGS += -f elf64 -i KernelLibC/inc