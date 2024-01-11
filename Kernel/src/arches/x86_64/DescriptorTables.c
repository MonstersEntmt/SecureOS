#include "DescriptorTables.h"

uint64_t        g_GDT[512];
struct IDTEntry g_IDT[256];

bool ClearSegment(uint16_t segment)
{
	if (segment >= 512)
		return false;
	g_GDT[segment] = 0;
	return true;
}

bool SetCodeSegment(uint16_t segment, uint8_t privilege, bool conforming)
{
	if (segment >= 512)
		return false;
	privilege     &= 3;
	g_GDT[segment] = 0x00AF'9B00'0000'FFFFUL | ((uint64_t) privilege << 45) | ((conforming ? 1UL : 0UL) << 42);
	return true;
}

bool SetDataSegment(uint16_t segment)
{
	if (segment >= 512)
		return false;
	g_GDT[segment] = 0x00AF'9300'0000'FFFFUL;
	return true;
}

void SetInterruptHandler(uint8_t vector, uint16_t selector, uint8_t ist, uint8_t privilege, void (*handler)(struct InterruptHandlerData* interruptData))
{
	uint64_t         addr  = (uint64_t) handler;
	struct IDTEntry* entry = g_IDT + vector;
	entry->low             = 0x0000'8E00'0000'0000UL | (addr & 0xFFFF) | (selector << 16) | ((uint64_t) ist << 32) | ((uint64_t) privilege << 45) | ((addr & 0xFFFF'0000) << 32);
	entry->high            = addr >> 32;
}

void SetTrapHandler(uint8_t vector, uint16_t selector, uint8_t ist, uint8_t privilege, void (*handler)(struct TrapHandlerData* trapData))
{
	uint64_t         addr  = (uint64_t) handler;
	struct IDTEntry* entry = g_IDT + vector;
	entry->low             = 0x0000'8F00'0000'0000UL | (addr & 0xFFFF) | (selector << 16) | ((uint64_t) ist << 32) | ((uint64_t) privilege << 45) | ((addr & 0xFFFF'0000) << 32);
	entry->high            = addr >> 32;
}