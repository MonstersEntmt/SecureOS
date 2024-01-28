KERNEL_LIBC_C_SRCS   := $(shell find KernelLibC/src/ -name '*.c' -o -path '*/arches' -prune -a -not -path '*/arches')
KERNEL_LIBC_ASM_SRCS :=

KERNEL_LIBC_CFLAGS   := -std=c23 -fno-builtin -nostdinc -nostdlib -IKernelLibC/inc
KERNEL_LIBC_ASMFLAGS :=
KERNEL_LIBC_LDFLAGS  := -e kernel_entry

include KernelLibC/Arches/$(ARCH).mk

KERNEL_LIBC_C_OBJS   := $(KERNEL_LIBC_C_SRCS:%=Bin-Int/$(CONFIG)/%.o)
KERNEL_LIBC_ASM_OBJS := $(KERNEL_LIBC_ASM_SRCS:%=Bin-Int/$(CONFIG)/%.o)

KERNEL_LIBC_OBJS := KERNEL_C_OBJS KERNEL_ASM_OBJS

ifeq ($(CONFIG), debug)
KERNEL_LIBC_CFLAGS += -O0 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
KERNEL_LIBC_ASMFLAGS += -O0 -gdwarf -D BUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), release)
KERNEL_LIBC_CFLAGS += -O3 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
KERNEL_LIBC_ASMFLAGS += -Ox -gdwarf -D BUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), dist)
KERNEL_LIBC_CFLAGS += -O3 -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
KERNEL_LIBC_ASMFLAGS += -Ox -D BUILD_CONFIG=BUILD_CONFIG_DEBUG
endif

$(KERNEL_LIBC_C_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(CC) $(KERNEL_LIBC_CFLAGS) -c -o $@ $<
	echo Compiled $<

$(KERNEL_LIBC_ASM_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(ASM) $(KERNEL_LIBC_ASMFLAGS) -o $@ $<
	echo Assembled $<