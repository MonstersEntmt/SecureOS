#include "x86_64/GDT.h"

#include <string.h>

alignas(16) uint64_t g_x86_64GDT[512];
alignas(16) uint64_t g_x86_64GDTR[2];

bool x86_64GDTClearDescriptors(void)
{
	memset(g_x86_64GDT, 0, sizeof(g_x86_64GDT));
	return true;
}

bool x86_64GDTSetNullDescriptor(uint16_t descriptor)
{
	if (descriptor >= 512)
		return false;
	g_x86_64GDT[descriptor] = 0;
	return true;
}

bool x86_64GDTSetCodeDescriptor(uint16_t descriptor, uint8_t privilege, bool conforming)
{
	if (descriptor >= 512)
		return false;
	g_x86_64GDT[descriptor] = 0x00AF'9B00'0000'FFFFUL | ((uint64_t) (privilege & 3) << 45) | ((conforming ? 1UL : 0UL) << 42);
	return true;
}

bool x86_64GDTSetDataDescriptor(uint16_t descriptor)
{
	if (descriptor >= 512)
		return false;
	g_x86_64GDT[descriptor] = 0x00AF'9300'0000'FFFFUL;
	return true;
}