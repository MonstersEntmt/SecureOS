#include "VMM.h"

uint64_t VMMArchConstructPageTableEntry(void* physicalAddress, enum VMMPageType type, enum VMMPageProtect protect)
{
	uint64_t entry = physicalAddress != nullptr ? 1 : 0;
	switch (type)
	{
	case VMM_PAGE_TYPE_4KIB: entry = (uint64_t) physicalAddress & 0xF'FFFF'FFFF'F000UL; break;
	case VMM_PAGE_TYPE_2MIB: entry = (uint64_t) physicalAddress & 0xF'FFFF'FFE0'0000UL | 0x80; break;
	case VMM_PAGE_TYPE_1GIB: entry = (uint64_t) physicalAddress & 0xF'FFFF'C000'0000UL | 0x80; break;
	}
	switch (protect)
	{
	case VMM_PAGE_PROTECT_READ_WRITE: entry |= 0x8000'0000'0000'0002; break;
	case VMM_PAGE_PROTECT_READ_ONLY: entry |= 0x8000'0000'0000'0000; break;
	case VMM_PAGE_PROTECT_READ_EXECUTE: entry |= 0x0000'0000'0000'0000; break;
	case VMM_PAGE_PROTECT_READ_WRITE_EXECUTE: entry |= 0x0000'0000'0000'0002; break;
	}
	return entry;
}

uint64_t VMMArchConstructPageTablePointer(uint64_t* subTableAddress)
{
	return 0x3 | ((uint64_t) subTableAddress & 0xF'FFFF'FFFF'F000UL);
}

void VMMArchGetPageTableEntry(uint64_t entry, uint8_t level, void** physicalAddress, enum VMMPageType* type)
{
	if (!physicalAddress || !type)
		return;
	if (level == 0)
	{
		*physicalAddress = (void*) (entry & 0xF'FFFF'FFFF'F000UL);
		*type            = VMM_PAGE_TYPE_4KIB;
	}
	else if (level == 1)
	{
		*physicalAddress = (void*) (entry & 0xF'FFFF'FFE0'0000UL);
		*type            = VMM_PAGE_TYPE_2MIB;
	}
	else if (level == 2)
	{
		*physicalAddress = (void*) (entry & 0xF'FFFF'C000'0000UL);
		*type            = VMM_PAGE_TYPE_1GIB;
	}
}

uint64_t* VMMArchGetPageTablePointer(uint64_t entry)
{
	return (uint64_t*) (entry & 0xF'FFFF'FFFF'F000UL);
}