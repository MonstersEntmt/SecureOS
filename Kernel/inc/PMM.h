#pragma once

#include <stddef.h>
#include <stdint.h>

enum PMMMemoryMapType
{
	PMMMemoryMapTypeInvalid           = 0x00,
	PMMMemoryMapTypeFree              = 0x01,
	PMMMemoryMapTypeReclaimable       = 0x11,
	PMMMemoryMapTypeLoaderReclaimable = 0x21,
	PMMMemoryMapTypeKernel            = 0x02,
	PMMMemoryMapTypeModule            = 0x12,
	PMMMemoryMapTypeReserved          = 0x04,
	PMMMemoryMapTypeNVS               = 0x14
};

struct PMMMemoryMapEntry
{
	uint64_t Start;
	uint64_t Size;

	enum PMMMemoryMapType Type;
};

typedef bool (*PMMGetMemoryMapEntryFn)(void* userdata, size_t index, struct PMMMemoryMapEntry* entry);

void  PMMInit(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata);
void  PMMPrintFreeList();
void* PMMAlloc();
void  PMMFree(void* address);
void* PMMAllocContiguous(size_t count);
void  PMMFreeContiguous(void* address, size_t count);