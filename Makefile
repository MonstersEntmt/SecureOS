.SILENT:

CONFIG ?= dist
ARCH   ?= x86_64

CC  := clang-18
CXX := clang++-18
LD  := ld.lld-18

QEMU_ARGS  :=
QEMU_FILES :=
BOOT_FILES :=

include Arches/$(ARCH).mk

include Tools/make.mk
include Kernel/make.mk

.PHONY: all configure clean run
all: Tools Kernel

configure: all

# Is rm -rf all that good here?
clean:
	rm -rf Bin/
	rm -rf Bin-Int/

BOOT_FILES += Bin/$(CONFIG)/UEFI/hyper.cfg
BOOT_FILES += Bin/$(CONFIG)/UEFI/secure-os/basic-latin.font
Bin/$(CONFIG)/UEFI/hyper.cfg: BootFiles/hyper.cfg
	mkdir -p $(dir $@)
	cp -T $< $@
	echo Copied hyper.cfg

Bin/$(CONFIG)/UEFI/secure-os/basic-latin.font: BootFiles/BasicLatin.font BootFiles/Font/BasicLatin.bmp BootFiles/Font/U+FFD.bmp
	mkdir -p $(dir $@)
	$(FONTGEN) -o $@ -- BootFiles/BasicLatin.font
	echo Font Generated basic-latin.font

QEMU_FILES += Bin/$(CONFIG)/BootFiles/Drive.img
Bin/$(CONFIG)/BootFiles/Drive.img: all $(BOOT_FILES)
	mkdir -p $(dir $@)
	$(IMGGEN) \
		--output path=$@ size=2GiB \
		--partition start=, end=+550MiB type=EF00 'name=EFI Partition' \
		--partition start=, end=. type=d236c553-8661-4925-907d-2fbd318e038b 'name=Primary Partition' \
		--format partition=1 type=FAT \
		--copy partition=1 from=Bin/$(CONFIG)/UEFI/ to=/
	echo Image Generated Drive.img

QEMU_ARGS += -drive format=raw,file=Bin/$(CONFIG)/BootFiles/Drive.img
QEMU_ARGS += --vga vmware -m 128M -smp sockets=1,cpus=4,maxcpus=4,cores=4,threads=1 -s -S
ifeq ($(CONFIG), debug)
QEMU_ARGS += -debugcon stdio -d int
else ifeq ($(CONFIG), release)
QEMU_ARGS += -debugcon stdio
endif

run: $(QEMU_FILES)
	$(QEMU) $(QEMU_ARGS)