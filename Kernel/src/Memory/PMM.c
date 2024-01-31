#include "Memory/PMM.h"
#include "Memory/FreeLUT/FreeLUTPMM.h"
#include "Panic.h"

#include <stdio.h>
#include <string.h>

struct PMMImplementation
{
	const char* Name;
	void (*Init)(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata);
	void (*Reclaim)(void);
	void (*GetMemoryStats)(struct PMMMemoryStats* stats);
	size_t (*GetMemoryMap)(const struct PMMMemoryMapEntry** entries);
	void* (*Alloc)(size_t count, uint8_t alignment, void* largestAddress);
	void (*Free)(void* address, size_t count);
};

static const struct PMMImplementation s_PMMImplementations[] = {
	(struct PMMImplementation) {
								.Name           = "freelut",
								.Init           = &FreeLUTPMMInit,
								.Reclaim        = &FreeLUTPMMReclaim,
								.GetMemoryStats = &FreeLUTPMMGetMemoryStats,
								.GetMemoryMap   = &FreeLUTPMMGetMemoryMap,
								.Alloc          = &FreeLUTPMMAlloc,
								.Free           = &FreeLUTPMMFree,
								}
};

static const struct PMMImplementation* s_PMMImpl = nullptr;

void PMMSelect(const char* name, size_t nameLen)
{
	for (size_t i = 0; !s_PMMImpl && i < sizeof(s_PMMImplementations) / sizeof(*s_PMMImplementations); ++i)
	{
		size_t implLen = strlen(s_PMMImplementations[i].Name);
		if (nameLen != implLen)
			continue;
		if (memcmp(name, s_PMMImplementations[i].Name, nameLen) == 0)
			s_PMMImpl = &s_PMMImplementations[i];
	}
	if (!s_PMMImpl)
		s_PMMImpl = &s_PMMImplementations[0];
}

const char* PMMGetSelectedName(void)
{
	return s_PMMImpl->Name;
}

void PMMInit(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata)
{
	{
		struct PMMMemoryMapEntry tempEntry;
		uintptr_t                currentAddress = 0;
		for (size_t i = 0; i < entryCount; ++i)
		{
			if (!getter(userdata, i, &tempEntry))
			{
				puts("CRITICAL: PMMInit received broken memory map callback");
				KernelPanic();
			}
			if (tempEntry.Start & 0x0FFFUL || tempEntry.Size & 0x0FFFUL)
			{
				puts("CRITICAL: PMMInit expects all memory map entries to be page aligned");
				KernelPanic();
			}
			if (tempEntry.Start < currentAddress)
			{
				puts("CRITICAL: PMMInit received memory map entries in non sorted state");
				KernelPanic();
			}
			if (i == 0 && (tempEntry.Start != 0 || tempEntry.Size < 0x3000))
			{
				puts("CRITICAL: PMMInit expects first three pages to be free");
				KernelPanic();
			}
			currentAddress = tempEntry.Start + tempEntry.Size;
		}
	}

	s_PMMImpl->Init(entryCount, getter, userdata);
}

void PMMReclaim(void)
{
	s_PMMImpl->Reclaim();
}

void PMMGetMemoryStats(struct PMMMemoryStats* stats)
{
	s_PMMImpl->GetMemoryStats(stats);
}

size_t PMMGetMemoryMap(const struct PMMMemoryMapEntry** entries)
{
	return s_PMMImpl->GetMemoryMap(entries);
}

void* PMMAlloc(size_t count)
{
	return s_PMMImpl->Alloc(count, 12, (void*) -1);
}

void* PMMAllocAligned(size_t count, uint8_t alignment)
{
	return s_PMMImpl->Alloc(count, alignment < 12 ? 12 : alignment, (void*) -1);
}

void* PMMAllocBelow(size_t count, void* largestAddress)
{
	return s_PMMImpl->Alloc(count, 12, largestAddress);
}

void* PMMAllocAlignedBelow(size_t count, uint8_t alignment, void* largestAddress)
{
	return s_PMMImpl->Alloc(count, alignment < 12 ? 12 : alignment, largestAddress);
}

void PMMFree(void* address, size_t count)
{
	s_PMMImpl->Free(address, count);
}