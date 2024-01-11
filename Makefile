.SILENT:

CONFIG ?= release
TARGET ?= x86_64

ifeq ($(TARGET), x86_64)
endif

ifeq ($(CONFIG), debug)
else ifeq ($(CONFIG), release)
else ifeq ($(CONFIG), dist)
endif

CC := clang-18
CXX := clang++-18
LD := ld.lld-18

BOOT_FILES :=
QEMU_ARGS :=

include Targets/$(TARGET).mk

include Boot/make.mk
include Kernel/make.mk

BOOT_FILES += Bin/UEFI/hyper.cfg
Bin/UEFI/hyper.cfg: Boot/hyper.cfg
	mkdir -p $(dir $@)
	cp -T $< $@

Bin/Boot/Drive.img: Kernel ImgGen $(BOOT_FILES)
	mkdir -p $(dir $@)
	Bin/Boot/ImgGen \
		-o $@ \
		-s 2GiB \
		-p 'start=~,end=+550MiB,type=EF00,name="EFI Partition"' \
		-p 'start=~,end=^,type=d236c553-8661-4925-907d-2fbd318e038b,name="Primary Partition"' \
		-f 'p=1,type=FAT32' \
		-c 'p=1,from=Bin/UEFI/,to=/'
	echo Created Drive.img

Drive.img: Bin/Boot/Drive.img

all: Kernel

QEMU_ARGS += -drive format=raw,file=Bin/Boot/Drive.img
QEMU_ARGS += -vga vmware -debugcon stdio
run: Drive.img
	$(QEMU) $(QEMU_ARGS)

clean:
	rm -rf Bin/
	rm -rf Bin-Int/