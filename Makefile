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

.PHONY: all
all: Kernel

.PHONY: clean
clean:
	rm -rf Bin/
	rm -rf Bin-Int/

include Targets/$(TARGET).mk

include Boot/make.mk
include FontBitmap/make.mk
include Kernel/make.mk

BOOT_FILES += Bin/$(CONFIG)/UEFI/hyper.cfg
Bin/$(CONFIG)/UEFI/hyper.cfg: Boot/hyper.cfg
	mkdir -p $(dir $@)
	cp -T $< $@

Bin/$(CONFIG)/Boot/Drive.img: Kernel ImgGen FontBitmap $(BOOT_FILES)
	mkdir -p $(dir $@)
	mkdir -p $(dir Bin/$(CONFIG)/UEFI/secure-os/)
	Bin/$(CONFIG)/FontBitmap FontBitmap/BasicLatin.font 'Bin/$(CONFIG)/UEFI/secure-os/bsclatin.fnt'
	Bin/$(CONFIG)/Boot/ImgGen \
		-o $@ \
		-s 2GiB \
		-p 'start=~,end=+550MiB,type=EF00,name="EFI Partition"' \
		-p 'start=~,end=^,type=d236c553-8661-4925-907d-2fbd318e038b,name="Primary Partition"' \
		-f 'p=1,type=FAT32' \
		-c 'p=1,from=Bin/$(CONFIG)/UEFI/,to=/'
	echo Created Drive.img

Drive.img: Bin/$(CONFIG)/Boot/Drive.img

QEMU_ARGS += -drive format=raw,file=Bin/$(CONFIG)/Boot/Drive.img
QEMU_ARGS += --vga vmware -debugcon stdio -m 128M -smp sockets=1,cpus=4,maxcpus=4,cores=4,threads=1
ifeq ($(CONFIG), debug)
QEMU_ARGS += -s -S
else ifeq ($(CONFIG), release)
QEMU_ARGS += -s -S
endif
run: Drive.img
	$(QEMU) $(QEMU_ARGS)

configure: Kernel ImgGen FontBitmap