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

void  PMMSetupMemoryMap(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata);
void  PMMInit();
void  PMMPrintFreeList();
void  PMMPrintBitmap(uint64_t firstPage, uint64_t lastPage);
void* PMMAlloc();
void  PMMFree(void* address);
void* PMMAllocContiguous(size_t count);
void  PMMFreeContiguous(void* address, size_t count);