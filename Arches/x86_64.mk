ASM  := nasm
QEMU := qemu-system-x86_64

QEMU_ARGS += -drive if=pflash,format=raw,readonly=on,file=BootFiles/OVMF_CODE-pure-efi.fd
QEMU_ARGS += -drive if=pflash,format=raw,file=Bin/$(CONFIG)/BootFiles/OVMF_VARS-pure-efi.fd

BOOT_FILES += Bin/$(CONFIG)/UEFI/EFI/BOOT/BOOTX64.EFI
Bin/$(CONFIG)/UEFI/EFI/BOOT/BOOTX64.EFI: BootFiles/BOOTX64.EFI
	mkdir -p $(dir $@)
	cp -T $< $@

QEMU_FILES += Bin/$(CONFIG)/BootFiles/OVMF_VARS-pure-efi.fd
Bin/$(CONFIG)/BootFiles/OVMF_VARS-pure-efi.fd: BootFiles/OVMF_VARS-pure-efi.fd
	mkdir -p $(dir $@)
	cp -T $< $@