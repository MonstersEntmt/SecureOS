.SILENT:

CONFIG = debug

CC := clang-18
CXX := clang++-18
LD := ld.lld-18

include Boot/make.mk
include Kernel/make.mk

all: Bin/UEFI/secure-os/kernel.elf Bin/Boot/ImgGen

Bin/Boot/Drive.img: Bin/UEFI/secure-os/kernel.elf Bin/Boot/ImgGen Boot/hyper.cfg Boot/BOOTX64.EFI
	mkdir -p Bin/UEFI/EFI/BOOT/
	cp -T Boot/hyper.cfg Bin/UEFI/hyper.cfg
	cp -T Boot/BOOTX64.EFI Bin/UEFI/EFI/BOOT/BOOTX64.EFI
	./Bin/Boot/ImgGen \
		-o Bin/Boot/Drive.img \
		-s 2GiB \
		-p 'start=~,end=+550MiB,type=EF00,name="EFI Partition"' \
		-p 'start=~,end=^,type=d236c553-8661-4925-907d-2fbd318e038b,name="Primary Partition"' \
		-f 'p=1,type=FAT32' \
		-c 'p=1,from=Bin/UEFI/,to=/'
	echo Created disk image

img: Bin/Boot/Drive.img

run: img
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=Boot/OVMF_CODE-pure-efi.fd \
		-drive if=pflash,format=raw,file=Boot/OVMF_VARS-pure-efi.fd \
		-drive format=raw,file=Bin/Boot/Drive.img \
		-vga vmware \
		-debugcon stdio

clean:
	rm -rf Bin/
	rm -rf Bin-Int/