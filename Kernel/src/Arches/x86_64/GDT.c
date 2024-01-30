#include "Arches/x86_64/GDT.h"

alignas(16) uint64_t g_GDT[512] = {};

bool GDTSetNullDescriptor(uint16_t descriptor)
{
	if (descriptor >= 512 * 8)
		return false;
	g_GDT[descriptor >> 3] = 0;
	return true;
}

bool GDTSetCodeDescriptor(uint16_t descriptor, uint8_t privilege, bool conforming)
{
	if (descriptor >= 512 * 8)
		return false;
	g_GDT[descriptor >> 3] = 0x00AF'9B00'0000'FFFFUL | ((uint64_t) (privilege & 3) << 45) | ((conforming ? 1UL : 0UL) << 42);
	return true;
}

bool GDTSetDataDescriptor(uint16_t descriptor)
{
	if (descriptor >= 512 * 8)
		return false;
	g_GDT[descriptor >> 3] = 0x00AF'9300'0000'FFFFUL;
	return true;
}

bool GDTSetLDTDescriptor(uint16_t descriptor, uint8_t privilege, void* address, uint32_t limit)
{
	if (descriptor >= 512 * 8 - 1)
		return false;
	uint64_t addr    = (uint64_t) address;
	uint16_t entry   = descriptor >> 3;
	g_GDT[entry]     = 0x0000'8200'0000'0000 | ((addr & 0xFF00'0000) << 32) | ((uint64_t) (limit & 0xF'0000) << 32) | ((uint64_t) (privilege & 3) << 45) | ((addr & 0xFF'FFFF) << 16) | (limit & 0xFFFF);
	g_GDT[entry + 1] = addr >> 32;
	return true;
}

void* GDTGet(void)
{
	return g_GDT;
}

void* LDTGet(uint16_t descriptor)
{
	if (descriptor >= 512 * 8 - 1)
		return nullptr;
	uint16_t entry = descriptor >> 3;
	uint64_t v     = g_GDT[entry];
	if (((v >> 20) & 0xF) != 0x2)
		return nullptr;
	uint64_t addr = (g_GDT[entry + 1] & 0xFFFF'FFFF) << 32 | ((v & 0xFF00'0000'0000'0000) >> 32) | ((v & 0x0000'00FF'FFFF'0000) >> 16);
	return (void*) addr;
}