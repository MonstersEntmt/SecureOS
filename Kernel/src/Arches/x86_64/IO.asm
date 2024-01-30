%include "Arches/x86_64/Build.asm"

GlobalLabel RawIn8 ; uint8_t RawIn8(uint16_t port)
    ; di => uint16_t port
    ; al => return uint8_t
    mov dx, di
    in al, dx
    ret

GlobalLabel RawIn16 ; uint16_t RawIn16(uint16_t port)
    ; di => uint16_t port
    ; ax => return uint16_t
    mov dx, di
    in ax, dx
    ret

GlobalLabel RawIn32 ; uint32_t RawIn32(uint16_t port)
    ; di  => uint16_t port
    ; eax => return uint32_t
    mov dx, di
    in eax, dx
    ret

GlobalLabel RawInBytes ; void RawInBytes(uint16_t port, void* buffer, size_t size)
    ; di  => uint16_t port
    ; rsi => void* buffer
    ; rdx => size_t size
    mov rcx, rdx
    mov dx, di
    mov rdi, rsi
    cld
    rep insb
    ret

GlobalLabel RawOut8 ; void RawOut8(uint16_t port, uint8_t value)
    ; di  => uint16_t port
    ; si8 => uint8_t value
    mov dx, di
    mov ax, si
    out dx, al
    ret

GlobalLabel RawOut16 ; void RawOut16(uint16_t port, uint16_t value)
    ; di => uint16_t port
    ; si => uint16_t value
    mov dx, di
    mov ax, si
    out dx, ax
    ret

GlobalLabel RawOut32 ; void RawOut32(uint16_t port, uint32_t value)
    ; di  => uint16_t port
    ; esi => uint32_t value
    mov dx, di
    mov eax, esi
    out dx, eax
    ret

GlobalLabel RawOutBytes ; void RawOutBytes(uint16_t port, const void* buffer, size_t size)
    ; di  => uint16_t port
    ; rsi => const void* buffer
    ; rdx => size_t size
    mov rcx, rdx
    mov dx, di
    cld
    rep outsb
    ret