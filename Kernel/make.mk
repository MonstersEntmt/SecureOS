KERNEL_C_SRCS   := $(shell find Kernel/src/ -name '*.c' -o -path '*/Arches' -prune -a -not -path '*/Arches')
KERNEL_ASM_SRCS :=

KERNEL_CFLAGS   := -std=c23 -fno-builtin -nostdinc -nostdlib -isystem KernelLibC/inc -IKernel/inc
KERNEL_ASMFLAGS :=
KERNEL_LDFLAGS  := -e kernel_entry

include Kernel/Arches/$(ARCH).mk

KERNEL_C_OBJS   := $(KERNEL_C_SRCS:%=Bin-Int/$(CONFIG)/%.o)
KERNEL_ASM_OBJS := $(KERNEL_ASM_SRCS:%=Bin-Int/$(CONFIG)/%.o)

KERNEL_OBJS := $(KERNEL_LIBC_OBJS) $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

ifeq ($(CONFIG), debug)
KERNEL_CFLAGS += -O0 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
KERNEL_ASMFLAGS += -O0 -gdwarf -D BUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), release)
KERNEL_CFLAGS += -O3 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
KERNEL_ASMFLAGS += -Ox -gdwarf -D BUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), dist)
KERNEL_CFLAGS += -O3 -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
KERNEL_ASMFLAGS += -Ox -D BUILD_CONFIG=BUILD_CONFIG_DEBUG
endif

$(KERNEL_C_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(CC) $(KERNEL_CFLAGS) -c -o $@ $<
	echo Compiled $<

$(KERNEL_ASM_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(ASM) $(KERNEL_ASMFLAGS) -o $@ $<
	echo Assembled $<

Bin/$(CONFIG)/UEFI/secure-os/kernel.elf: $(KERNEL_OBJS)
	mkdir -p $(dir $@)
	$(LD) $(KERNEL_LDFLAGS) -o $@ $(KERNEL_OBJS)
	echo Linked Kernel

.PHONY: Kernel
Kernel: Bin/$(CONFIG)/UEFI/secure-os/kernel.elf