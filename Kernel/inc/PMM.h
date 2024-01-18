#pragma once

#include <stddef.h>
#include <stdint.h>

enum PMMMemoryMapType
{
	PMMMemoryMapTypeInvalid           = 0x00,
	PMMMemoryMapTypeUsable            = 0x01,
	PMMMemoryMapTypeReclaimable       = 0x11,
	PMMMemoryMapTypeLoaderReclaimable = 0x21,
	PMMMemoryMapTypeTaken             = 0x02,
	PMMMemoryMapTypeNullGuard         = 0x12,
	PMMMemoryMapTypePMM               = 0x22,
	PMMMemoryMapTypeKernel            = 0x04,
	PMMMemoryMapTypeModule            = 0x14,
	PMMMemoryMapTypeReserved          = 0x08,
	PMMMemoryMapTypeACPI              = 0x18,
	PMMMemoryMapTypeNVS               = 0x28
};

struct PMMMemoryMapEntry
{
	uint64_t Start;
	uint64_t Size;

	enum PMMMemoryMapType Type;
};

struct PMMMemoryStats
{
	uint64_t AllocatorFootprint;

	uint64_t LastUsableAddress;
	uint64_t LastAddress;
	uint64_t PagesFree;
};

typedef bool (*PMMGetMemoryMapEntryFn)(void* userdata, size_t index, struct PMMMemoryMapEntry* entry);

void   PMMInit(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata);
void   PMMReclaim(void);
void   PMMGetMemoryStats(struct PMMMemoryStats* stats);
size_t PMMGetMemoryMap(const struct PMMMemoryMapEntry** entries);
void   PMMDebugPrint(void);

void* PMMAlloc(size_t count);
void* PMMAllocAligned(size_t count, uint8_t alignment);
void  PMMFree(void* address, size_t count);