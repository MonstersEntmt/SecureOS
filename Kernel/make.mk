KERNEL_LIBC_C_SRCS := $(shell find Kernel/clib/src/ -name '*.c' -o -path */arches -prune -a -not -path */arches)
KERNEL_LIBC_ASM_SRCS :=

KERNEL_C_SRCS := $(shell find Kernel/src/ -name '*.c' -o -path */arches -prune -a -not -path */arches)
KERNEL_ASM_SRCS :=

KERNEL_LIBC_CFLAGS := -std=c23 -fno-builtin -nostdinc -nostdlib -IKernel/clib/inc/
KERNEL_LIBC_ASMFLAGS :=

KERNEL_CFLAGS := -std=c23 -fno-builtin -nostdinc -nostdlib -isystem Kernel/clib/inc -IKernel/inc/
KERNEL_ASMFLAGS :=
KERNEL_LDFLAGS := -e kernel_entry

include Kernel/Targets/$(TARGET).mk

KERNEL_LIBC_C_OBJS := $(KERNEL_LIBC_C_SRCS:%=Bin-Int/$(CONFIG)/%.o)
KERNEL_LIBC_ASM_OBJS := $(KERNEL_LIBC_ASM_SRCS:%=Bin-Int/$(CONFIG)/%.o)
KERNEL_C_OBJS := $(KERNEL_C_SRCS:%=Bin-Int/$(CONFIG)/%.o)
KERNEL_ASM_OBJS := $(KERNEL_ASM_SRCS:%=Bin-Int/$(CONFIG)/%.o)

KERNEL_OBJS := $(KERNEL_LIBC_C_OBJS) $(KERNEL_LIBC_ASM_OBJS) $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

ifeq ($(CONFIG), debug)
KERNEL_LIBC_CFLAGS += -O0 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
KERNEL_CFLAGS += -O0 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
KERNEL_ASMFLAGS += -O0 -g -D BUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), release)
KERNEL_LIBC_CFLAGS += -O3 -g -DBUILD_CONFIG=BUILD_CONFIG_RELEASE
KERNEL_CFLAGS += -O3 -g -DBUILD_CONFIG=BUILD_CONFIG_RELEASE
KERNEL_ASMFLAGS += -Ox -g -D BUILD_CONFIG=BUILD_CONFIG_RELEASE
else ifeq ($(CONFIG), dist)
KERNEL_LIBC_CFLAGS += -O3 -DBUILD_CONFIG=BUILD_CONFIG_DIST
KERNEL_CFLAGS += -O3 -DBUILD_CONFIG=BUILD_CONFIG_DIST
KERNEL_ASMFLAGS += -Ox -D BUILD_CONFIG=BUILD_CONFIG_DIST
endif

$(KERNEL_LIBC_C_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(CC) $(KERNEL_LIBC_CFLAGS) -c -o $@ $<
	echo Compiled $<

$(KERNEL_LIBC_ASM_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(ASM) $(KERNEL_LIBC_ASMFLAGS) -o $@ $<
	echo Assembled $<

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

Kernel: Bin/$(CONFIG)/UEFI/secure-os/kernel.elf