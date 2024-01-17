#pragma once

#include <stddef.h>
#include <stdint.h>

enum VMMPageType
{
	VMM_PAGE_TYPE_4KIB = 0,
	VMM_PAGE_TYPE_2MIB,
	VMM_PAGE_TYPE_1GIB
};

enum VMMPageProtect
{
	VMM_PAGE_PROTECT_READ_WRITE = 0,
	VMM_PAGE_PROTECT_READ_ONLY,
	VMM_PAGE_PROTECT_READ_EXECUTE,
	VMM_PAGE_PROTECT_READ_WRITE_EXECUTE,
};

struct VMMMemoryStats
{
	uint64_t AllocatorFootprint;

	uint64_t PagesAllocated;
};

void* VMMNewPageTable();
void  VMMFreePageTable(void* pageTable);
void  VMMGetMemoryStats(void* pageTable, struct VMMMemoryStats* stats);

void* VMMAlloc(void* pageTable, size_t count, uint8_t alignment, enum VMMPageType type, enum VMMPageProtect protect);
void* VMMAllocAt(void* pageTable, uint64_t virtualAddress, size_t count, enum VMMPageType type, enum VMMPageProtect protect);
void  VMMFree(void* pageTable, void* virtualAddress, size_t count);

void VMMProtect(void* pageTable, void* virtualAddress, size_t count, enum VMMPageProtect protect);