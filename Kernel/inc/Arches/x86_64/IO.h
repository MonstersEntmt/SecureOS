#pragma once

#include <stddef.h>
#include <stdint.h>

uint8_t  RawIn8(uint16_t port);
uint16_t RawIn16(uint16_t port);
uint32_t RawIn32(uint16_t port);
void     RawInBytes(uint16_t port, void* buffer, size_t size);
void     RawOut8(uint16_t port, uint8_t value);
void     RawOut16(uint16_t port, uint16_t value);
void     RawOut32(uint16_t port, uint32_t value);
void     RawOutBytes(uint16_t port, const void* buffer, size_t size);