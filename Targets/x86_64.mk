ASM := nasm
QEMU := qemu-system-x86_64

QEMU_ARGS += -drive if=pflash,format=raw,readonly=on,file=Boot/OVMF_CODE-pure-efi.fd
QEMU_ARGS += -drive if=pflash,format=raw,file=Boot/OVMF_VARS-pure-efi.fd

BOOT_FILES += Bin/UEFI/EFI/BOOT/BOOTX64.EFI

Bin/UEFI/EFI/BOOT/BOOTX64.EFI: Boot/BOOTX64.EFI
	mkdir -p $(dir $@)
	cp -T $< $@