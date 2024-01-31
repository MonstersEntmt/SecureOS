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
	PMMMemoryMapTypeTrampoline        = 0x22,
	PMMMemoryMapTypeKernel            = 0x04,
	PMMMemoryMapTypeModule            = 0x14,
	PMMMemoryMapTypePMM               = 0x24,
	PMMMemoryMapTypeReserved          = 0x08,
	PMMMemoryMapTypeACPI              = 0x18,
	PMMMemoryMapTypeNVS               = 0x28
};

struct PMMMemoryMapEntry
{
	uintptr_t Start;
	size_t    Size;

	enum PMMMemoryMapType Type;
};

struct PMMMemoryStats
{
	void*  Address;
	size_t Footprint;

	void*  LastUsableAddress;
	void*  LastPhysicalAddress;
	size_t PagesTaken;
	size_t PagesFree;

	size_t AllocCalls;
	size_t FreeCalls;
};

typedef bool (*PMMGetMemoryMapEntryFn)(void* userdata, size_t index, struct PMMMemoryMapEntry* entry);

void        PMMSelect(const char* name, size_t nameLen);
const char* PMMGetSelectedName(void);
void        PMMInit(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata);
void        PMMReclaim(void);
void        PMMGetMemoryStats(struct PMMMemoryStats* stats);
size_t      PMMGetMemoryMap(const struct PMMMemoryMapEntry** entries);

void* PMMAlloc(size_t count);
void* PMMAllocAligned(size_t count, uint8_t alignment);
void* PMMAllocBelow(size_t count, void* largestAddress);
void* PMMAllocAlignedBelow(size_t count, uint8_t alignment, void* largestAddress);
void  PMMFree(void* address, size_t count);