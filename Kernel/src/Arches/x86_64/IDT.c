#include "Arches/x86_64/IDT.h"

struct IDTEntry
{
	uint64_t Lower;
	uint64_t Upper;
};

alignas(16) struct IDTEntry g_IDT[256];

bool IDTClearVector(uint16_t vector)
{
	if (vector >= 256)
		return false;
	g_IDT[vector] = (struct IDTEntry) {
		.Lower = 0,
		.Upper = 0
	};
	return true;
}

bool IDTSetCallGate(uint16_t vector, void* target, uint16_t selector, uint8_t privilege)
{
	if (vector >= 256)
		return false;
	uint64_t addr = (uint64_t) target;
	g_IDT[vector] = (struct IDTEntry) {
		.Lower = 0x0000'8C00'0000'0000UL | (addr & 0xFFFF) | (selector << 16) | ((uint64_t) (privilege & 3) << 45) | ((addr & 0xFFFF'0000) << 32),
		.Upper = addr >> 32
	};
	return true;
}

bool IDTSetInterruptGate(uint16_t vector, void* target, uint16_t selector, uint8_t privilege, uint8_t ist)
{
	if (vector >= 256)
		return false;
	uint64_t addr = (uint64_t) target;
	g_IDT[vector] = (struct IDTEntry) {
		.Lower = 0x0000'8E00'0000'0000UL | (addr & 0xFFFF) | (selector << 16) | ((uint64_t) (ist & 7) << 32) | ((uint64_t) (privilege & 3) << 45) | ((addr & 0xFFFF'0000) << 32),
		.Upper = addr >> 32
	};
	return true;
}

bool IDTSetTrapGate(uint16_t vector, void* target, uint16_t selector, uint8_t privilege, uint8_t ist)
{
	if (vector >= 256)
		return false;
	uint64_t addr = (uint64_t) target;
	g_IDT[vector] = (struct IDTEntry) {
		.Lower = 0x0000'8F00'0000'0000UL | (addr & 0xFFFF) | (selector << 16) | ((uint64_t) (ist & 7) << 32) | ((uint64_t) (privilege & 3) << 45) | ((addr & 0xFFFF'0000) << 32),
		.Upper = addr >> 32
	};
	return true;
}

void* IDTGet(void)
{
	return g_IDT;
}