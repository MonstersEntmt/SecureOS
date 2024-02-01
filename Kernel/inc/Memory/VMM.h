#pragma once

#include <stddef.h>
#include <stdint.h>

#define VMM_PAGE_SIZE_4KiB     0x0000'0000
#define VMM_PAGE_SIZE_2MiB     0x0000'0001
#define VMM_PAGE_SIZE_1GiB     0x0000'0002
#define VMM_PAGE_SIZE_RESERVED 0x0000'0003
#define VMM_PAGE_SIZE_BITS     0x0000'000F

#define VMM_PAGE_PROTECT_READ_WRITE         0x0000'0000
#define VMM_PAGE_PROTECT_READ_ONLY          0x0000'0010
#define VMM_PAGE_PROTECT_READ_WRITE_EXECUTE 0x0000'0020
#define VMM_PAGE_PROTECT_READ_EXECUTE       (VMM_PAGE_PROTECT_READ_WRITE_EXECUTE | VMM_PAGE_PROTECT_READ_ONLY)
#define VMM_PAGE_PROTECT_BITS               0x0000'00F0

#define VMM_PAGE_AUTO_COMMIT 0x0000'0100
#define VMM_PAGE_OPTION_BITS 0x0000'0F00

struct VMMMemoryStats
{
	size_t Footprint;

	size_t PagesAllocated;
	size_t PagesMapped;
	size_t PagesMappedToDisk;

	size_t AllocCalls;
	size_t FreeCalls;
	size_t ProtectCalls;
	size_t MapCalls;
};

void        VMMSelect(const char* name, size_t nameLen);
const char* VMMGetSelectedName(void);
void*       VMMCreate(void);
void        VMMDestroy(void* vmm);
void        VMMGetMemoryStats(void* vmm, struct VMMMemoryStats* stats);

void* VMMAlloc(void* vmm, size_t count, uint8_t alignment, uint32_t flags);
void* VMMAllocAt(void* vmm, void* virtualAddress, size_t count, uint32_t flags);
void  VMMFree(void* vmm, void* virtualAddress, size_t count);

void  VMMProtect(void* vmm, void* virtualAddress, size_t count, uint32_t protect);
void  VMMMap(void* vmm, void* virtualAddress, void* physicalAddress);
void  VMMMapLinear(void* vmm, void* virtualAddress, void* physicalAddress, size_t count);
void* VMMTranslate(void* vmm, void* virtualAddress);

void  VMMActivate(void* vmm);
void* VMMGetRootTable(void* vmm, uint8_t* levels, bool* use1GiB);