#include "x86_64/IDT.h"

#include <string.h>

struct IDTEntry
{
	uint64_t lower, upper;
};

alignas(16) struct IDTEntry g_x86_64IDT[256];
alignas(16) uint64_t g_x86_64IDTR[2];

bool x86_64IDTClearDescriptors()
{
	memset(g_x86_64IDT, 0, sizeof(g_x86_64IDT));
	return true;
}

bool x86_64IDTSetCallGate(uint16_t vector, uint64_t target, uint16_t selector, uint8_t privilege)
{
	if (vector >= 256)
		return false;
	struct IDTEntry* entry = &g_x86_64IDT[vector];
	entry->lower           = 0x0000'8C00'0000'0000UL | (target & 0xFFFF) | (selector << 16) | ((uint64_t) (privilege & 3) << 45) | ((target & 0xFFFF'0000) << 32);
	entry->upper           = target >> 32;
	return true;
}

bool x86_64IDTSetInterruptGate(uint16_t vector, uint64_t target, uint16_t selector, uint8_t privilege, uint8_t ist)
{
	if (vector >= 256)
		return false;
	struct IDTEntry* entry = &g_x86_64IDT[vector];
	entry->lower           = 0x0000'8E00'0000'0000UL | (target & 0xFFFF) | (selector << 16) | ((uint64_t) (ist & 7) << 32) | ((uint64_t) (privilege & 3) << 45) | ((target & 0xFFFF'0000) << 32);
	entry->upper           = target >> 32;
	return true;
}

bool x86_64IDTSetTrapGate(uint16_t vector, uint64_t target, uint16_t selector, uint8_t privilege, uint8_t ist)
{
	if (vector >= 256)
		return false;
	struct IDTEntry* entry = &g_x86_64IDT[vector];
	entry->lower           = 0x0000'8F00'0000'0000UL | (target & 0xFFFF) | (selector << 16) | ((uint64_t) (ist & 7) << 32) | ((uint64_t) (privilege & 3) << 45) | ((target & 0xFFFF'0000) << 32);
	entry->upper           = target >> 32;
	return true;
}