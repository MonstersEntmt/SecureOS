LIBC_C_SRCS := $(shell find Kernel/clib/src/ -name '*.c')
LIBC_C_OBJS := $(LIBC_C_SRCS:%=Bin-Int/%.o)
LIBC_ASM_SRCS := $(shell find Kernel/clib/src/ -name '*.asm')
LIBC_ASM_OBJS := $(LIBC_ASM_SRCS:%=Bin-Int/%.o)
C_SRCS := $(shell find Kernel/src/ -name '*.c')
C_OBJS := $(C_SRCS:%=Bin-Int/%.o)
ASM_SRCS := $(shell find Kernel/src/ -name '*.asm')
ASM_OBJS := $(ASM_SRCS:%=Bin-Int/%.o)

LIBC_CFLAGS := -std=c23 -fno-builtin -nostdinc -nostdlib -IKernel/clib/inc/
LIBC_NASMFLAGS := -f elf64

KERNEL_CFLAGS    := -std=c23 -fno-builtin -nostdinc -nostdlib -isystem Kernel/clib/inc -IKernel/inc/
KERNEL_NASMFLAGS := -f elf64
KERNEL_LDFLAGS   := -e kernel_entry

Bin-Int/Kernel/clib/src/%.asm.o: Kernel/clib/src/%.asm
	mkdir -p $(dir $@)
	nasm $(LIBC_NASMFLAGS) -o $@ $<
	echo Assembled $<

Bin-Int/Kernel/clib/src/%.c.o: Kernel/clib/src/%.c
	mkdir -p $(dir $@)
	$(CC) $(LIBC_CFLAGS) -c -o $@ $<
	echo Built $<

Bin-Int/Kernel/src/%.asm.o: Kernel/src/%.asm
	mkdir -p $(dir $@)
	nasm $(KERNEL_NASMFLAGS) -o $@ $<
	echo Assembled $<

Bin-Int/Kernel/src/%.c.o: Kernel/src/%.c
	mkdir -p $(dir $@)
	$(CC) $(KERNEL_CFLAGS) -c -o $@ $<
	echo Built $<

Bin/UEFI/secure-os/kernel.elf: $(LIBC_C_OBJS) $(LIBC_ASM_OBJS) $(C_OBJS) $(ASM_OBJS)
	mkdir -p $(dir $@)
	$(LD) $(KERNEL_LDFLAGS) -o $@ $(LIBC_C_OBJS) $(LIBC_ASM_OBJS) $(C_OBJS) $(ASM_OBJS)
	echo Linked Kernel