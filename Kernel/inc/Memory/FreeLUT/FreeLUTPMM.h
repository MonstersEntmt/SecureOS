#pragma once

#include "Memory/PMM.h"

#include <stddef.h>

void   FreeLUTPMMInit(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata);
void   FreeLUTPMMReclaim(void);
void   FreeLUTPMMGetMemoryStats(struct PMMMemoryStats* stats);
size_t FreeLUTPMMGetMemoryMap(const struct PMMMemoryMapEntry** entries);
void*  FreeLUTPMMAlloc(size_t count, uint8_t alignment, void* largestAddress);
void   FreeLUTPMMFree(void* address, size_t count);