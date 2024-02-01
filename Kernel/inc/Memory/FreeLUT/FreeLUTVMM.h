#pragma once

#include "Memory/VMM.h"

#include <stddef.h>
#include <stdint.h>

void* FreeLUTVMMCreate(void);
void  FreeLUTVMMDestroy(void* vmm);
void  FreeLUTVMMGetMemoryStats(void* vmm, struct VMMMemoryStats* stats);

void* FreeLUTVMMAlloc(void* vmm, size_t count, uint8_t alignment, uint32_t flags);
void* FreeLUTVMMAllocAt(void* vmm, void* virtualAddress, size_t count, uint32_t flags);
void  FreeLUTVMMFree(void* vmm, void* virtualAddress, size_t count);

void  FreeLUTVMMProtect(void* vmm, void* virtualAddress, size_t count, uint32_t protect);
void  FreeLUTVMMMap(void* vmm, void* virtualAddress, void* physicalAddress);
void  FreeLUTVMMMapLinear(void* vmm, void* virtualAddress, void* physicalAddress, size_t count);
void* FreeLUTVMMTranslate(void* vmm, void* virtualAddress);

void* FreeLUTVMMGetRootTable(void* vmm, uint8_t* levels, bool* use1GiB);