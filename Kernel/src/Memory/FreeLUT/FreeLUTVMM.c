#include "Memory/FreeLUT/FreeLUTVMM.h"
#include "Build.h"
#include "Memory/FreeLUT/FreeLUT.h"
#include "Memory/PMM.h"
#include "Memory/VMM.h"
#include "Panic.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FREELUT_RANGE_FREE           0b000
#define FREELUT_RANGE_RESERVED1      0b001
#define FREELUT_RANGE_TBL_PTR        0b010
#define FREELUT_RANGE_MAPPED         0b011
#define FREELUT_RANGE_UNMAPPED       0b100
#define FREELUT_RANGE_AUTO_COMMIT    0b101
#define FREELUT_RANGE_MAPPED_TO_DISK 0b110
#define FREELUT_RANGE_RESERVED2      0b111

#define FREELUT_RANGE_BITS         0x0000'0000'0000'0007UL
#define FREELUT_RANGE_4KIB_ADDRESS 0x000F'FFFF'FFFF'F000UL
#define FREELUT_RANGE_32B_ADDRESS  0x000F'FFFF'FFFF'FFE0UL

#if BUILD_IS_ARCH_X86_64
	#define PAGE_TABLE_FREE 0x0000'0000'0000'0000UL

	#define PAGE_TABLE_4KIB_ADDRESS 0x000F'FFFF'FFFF'F000UL
	#define PAGE_TABLE_2MIB_ADDRESS 0x000F'FFFF'FFE0'0000UL
	#define PAGE_TABLE_1GIB_ADDRESS 0x000F'FFFF'C000'0000UL

	#define PAGE_TABLE_PRESENT   0x0000'0000'0000'0001UL
	#define PAGE_TABLE_4KIB_PAGE 0x0000'0000'0000'0000UL
	#define PAGE_TABLE_2MIB_PAGE 0x0000'0000'0000'0080UL
	#define PAGE_TABLE_1GIB_PAGE 0x0000'0000'0000'0080UL

	#define PAGE_TABLE_PROTECT_READ_WRITE         0x8000'0000'0000'0002UL
	#define PAGE_TABLE_PROTECT_READ_ONLY          0x8000'0000'0000'0000UL
	#define PAGE_TABLE_PROTECT_READ_WRITE_EXECUTE 0x0000'0000'0000'0002UL
	#define PAGE_TABLE_PROTECT_READ_EXECUTE       0x0000'0000'0000'0000UL
	#define PAGE_TABLE_PROTECT_BITS               0x8000'0000'0000'0002UL
#else
	#define PAGE_TABLE_FREE                       0
	#define PAGE_TABLE_4KIB_ADDRESS               0
	#define PAGE_TABLE_2MIB_ADDRESS               0
	#define PAGE_TABLE_1GIB_ADDRESS               0
	#define PAGE_TABLE_PRESENT                    0
	#define PAGE_TABLE_4KIB_PAGE                  0
	#define PAGE_TABLE_2MIB_PAGE                  0
	#define PAGE_TABLE_1GIB_PAGE                  0
	#define PAGE_TABLE_PROTECT_READ_WRITE         0
	#define PAGE_TABLE_PROTECT_READ_ONLY          0
	#define PAGE_TABLE_PROTECT_READ_WRITE_EXECUTE 0
	#define PAGE_TABLE_PROTECT_READ_EXECUTE       0
	#define PAGE_TABLE_PROTECT_BITS               0
#endif

struct FreeLUTEntry
{
	uintptr_t            Start;
	size_t               Count;
	struct FreeLUTEntry* Prev;
	struct FreeLUTEntry* Next;
};

struct FreeLUTPage
{
	uint64_t            Bitmap[2];
	struct FreeLUTPage* Prev;
	struct FreeLUTPage* Next;
	struct FreeLUTEntry Entries[127];
};

struct FreeLUTState
{
	struct VMMMemoryStats Stats;

	uint8_t Levels;
	bool    Use1GiB;

	uintptr_t* PageTableRoot;
	uintptr_t* RangeTableRoot;

	struct FreeLUTPage*  FirstPage;
	struct FreeLUTPage*  LastPage;
	struct FreeLUTEntry* Last;
	struct FreeLUTEntry* LUT[255];
};

static void FreeLUTLock(void* vmm)
{
}

static void FreeLUTUnlock(void* vmm)
{
}

static bool FreeLUTFreeMappedRange(struct FreeLUTState* state, uintptr_t pageEntry, uint8_t level)
{
	switch (level)
	{
	case 0:
		PMMFree((void*) (pageEntry & PAGE_TABLE_4KIB_ADDRESS), 1);
		--state->Stats.PagesMapped;
		return true;
	case 1:
		PMMFree((void*) (pageEntry & PAGE_TABLE_2MIB_ADDRESS), 1 << 9);
		state->Stats.PagesMapped -= 1 << 9;
		return true;
	case 2:
		if (!state->Use1GiB)
			return false;
		PMMFree((void*) (pageEntry & PAGE_TABLE_1GIB_ADDRESS), 1 << 18);
		state->Stats.PagesMapped -= 1 << 18;
		return true;
	default: return false;
	}
}

static bool FreeLUTFreeMappedToDiskRange(struct FreeLUTState* state, uintptr_t rangeEntry, uint8_t level)
{
	// TODO(MarcasRealAccount): Implement mapping to disk
	switch (level)
	{
	case 0:
		--state->Stats.PagesMappedToDisk;
		return true;
	case 1:
		state->Stats.PagesMappedToDisk -= 1 << 9;
		return true;
	case 2:
		if (!state->Use1GiB)
			return false;
		state->Stats.PagesMappedToDisk -= 1 << 18;
		return true;
	default: return false;
	}
}

static void FreeLUTFreeRecursive(struct FreeLUTState* state, uintptr_t* pageTable, uintptr_t* rangeTable, uint8_t level)
{
	for (uint16_t i = 0; i < 512; ++i)
	{
		uintptr_t rangeEntry = rangeTable[i];
		switch (rangeEntry & FREELUT_RANGE_BITS)
		{
		case FREELUT_RANGE_TBL_PTR:
			FreeLUTFreeRecursive(state, (uintptr_t*) (pageTable[i] & PAGE_TABLE_4KIB_ADDRESS), (uintptr_t*) (rangeEntry & FREELUT_RANGE_4KIB_ADDRESS), level - 1);
			break;
		case FREELUT_RANGE_MAPPED:
			if (!FreeLUTFreeMappedRange(state, pageTable[i], level))
				printf("FreeLUT WARN: Could not free mapped range in rangle table %p entry %hu on level %hhu", rangeTable, i, level);
			break;
		case FREELUT_RANGE_MAPPED_TO_DISK:
			if (!FreeLUTFreeMappedToDiskRange(state, rangeTable[i], level))
				printf("FreeLUT WARN: Could not free mapped to disk range in rangle table %p entry %hu on level %hhu", rangeTable, i, level);
			break;
		default:
			break;
		}
	}
	PMMFree(pageTable, 1);
	PMMFree(rangeTable, 1);
	state->Stats.Footprint -= 2;
}

static size_t FreeLUTMapLinearRecursive(struct FreeLUTState* state, uintptr_t* pageTable, uintptr_t* rangeTable, uintptr_t firstPage, uintptr_t lastPage, uintptr_t physicalAddress, uint8_t level)
{
	const uint8_t shift       = 9 * level;
	uint16_t      firstEntry  = (firstPage >> shift) & 511;
	uint16_t      lastEntry   = (lastPage >> shift) & 511;
	size_t        mappedCount = 0;
	uintptr_t     alignment   = ~0UL << (shift + 12);
	for (uint16_t i = firstEntry; i <= lastEntry; ++i)
	{
		const uintptr_t offset = ((uintptr_t) i - firstEntry) << (shift + 12);

		uintptr_t rangeEntry = rangeTable[i];
		switch (rangeEntry & FREELUT_RANGE_BITS)
		{
		case FREELUT_RANGE_TBL_PTR:
			mappedCount += FreeLUTMapLinearRecursive(
				state,
				(uintptr_t*) (pageTable[i] & PAGE_TABLE_4KIB_ADDRESS),
				(uintptr_t*) (rangeEntry & FREELUT_RANGE_4KIB_ADDRESS),
				i == firstEntry ? firstPage - (i << shift) : 0,
				i == lastEntry ? lastPage - (i << shift) : (1UL << shift) - 1,
				i == firstEntry ? physicalAddress : (physicalAddress + offset) & alignment,
				level - 1);
			break;
		case FREELUT_RANGE_MAPPED:
			switch (level)
			{
			case 0:
				pageTable[i] = (pageTable[i] & ~PAGE_TABLE_4KIB_ADDRESS) | ((physicalAddress + offset) & PAGE_TABLE_4KIB_ADDRESS);
				++mappedCount;
				break;
			case 1:
				pageTable[i] = (pageTable[i] & ~PAGE_TABLE_2MIB_ADDRESS) | ((physicalAddress + offset) & PAGE_TABLE_2MIB_ADDRESS);
				mappedCount += 1 << 9;
				break;
			case 2:
				pageTable[i] = (pageTable[i] & ~PAGE_TABLE_1GIB_ADDRESS) | ((physicalAddress + offset) & PAGE_TABLE_1GIB_ADDRESS);
				mappedCount += 1 << 18;
				break;
			}
			break;
		case FREELUT_RANGE_MAPPED_TO_DISK:
			switch (level)
			{
			case 0: --state->Stats.PagesMappedToDisk; break;
			case 1: state->Stats.PagesMappedToDisk -= 1 << 9; break;
			case 2: state->Stats.PagesMappedToDisk -= 1 << 18; break;
			}
			[[fallthrough]];
		case FREELUT_RANGE_UNMAPPED:
		case FREELUT_RANGE_AUTO_COMMIT:
			switch (level)
			{
			case 0:
				pageTable[i] = (pageTable[i] & ~PAGE_TABLE_4KIB_ADDRESS) | ((physicalAddress + offset) & PAGE_TABLE_4KIB_ADDRESS) | PAGE_TABLE_PRESENT;
				++state->Stats.PagesMapped;
				++mappedCount;
				break;
			case 1:
				pageTable[i]              = (pageTable[i] & ~PAGE_TABLE_2MIB_ADDRESS) | ((physicalAddress + offset) & PAGE_TABLE_2MIB_ADDRESS) | PAGE_TABLE_PRESENT;
				state->Stats.PagesMapped += 1 << 9;
				mappedCount              += 1 << 9;
				break;
			case 2:
				if (!state->Use1GiB)
				{
					printf("FreeLUT WARN: Range table %p entry %hu has a 1GiB unmapped range without 1GiB support\n", rangeTable, i);
					break;
				}
				pageTable[i]              = (pageTable[i] & ~PAGE_TABLE_1GIB_ADDRESS) | ((physicalAddress + offset) & PAGE_TABLE_1GIB_ADDRESS) | PAGE_TABLE_PRESENT;
				state->Stats.PagesMapped += 1 << 18;
				mappedCount              += 1 << 18;
				break;
			default:
				printf("FreeLUT WARN: Range table %p entry %hu has an unmapped range on level %hhu\n", rangeTable, i, level);
				break;
			}
			rangeTable[i] = FREELUT_RANGE_MAPPED;
			break;
		default: break;
		}
	}
	return mappedCount;
}

static size_t FreeLUTProtectRecursive(struct FreeLUTState* state, uintptr_t* pageTable, uintptr_t* rangeTable, uintptr_t firstPage, uintptr_t lastPage, uintptr_t protect, uint8_t level)
{
	const uint8_t shift        = 9 * level;
	uint16_t      firstEntry   = (firstPage >> shift) & 511;
	uint16_t      lastEntry    = (lastPage >> shift) & 511;
	size_t        protectCount = 0;
	for (uint16_t i = firstEntry; i <= lastEntry; ++i)
	{
		uintptr_t rangeEntry = rangeTable[i];
		switch (rangeEntry & FREELUT_RANGE_BITS)
		{
		case FREELUT_RANGE_TBL_PTR:
			protectCount += FreeLUTProtectRecursive(
				state,
				(uintptr_t*) (pageTable[i] & PAGE_TABLE_4KIB_ADDRESS),
				(uintptr_t*) (rangeEntry & FREELUT_RANGE_4KIB_ADDRESS),
				i == firstEntry ? firstPage - (i << shift) : 0,
				i == lastEntry ? lastPage - (i << shift) : (1UL << shift) - 1,
				protect,
				level - 1);
			break;
		case FREELUT_RANGE_MAPPED:
		case FREELUT_RANGE_MAPPED_TO_DISK:
		case FREELUT_RANGE_UNMAPPED:
		case FREELUT_RANGE_AUTO_COMMIT:
		{
			pageTable[i] = (pageTable[i] & ~PAGE_TABLE_PROTECT_BITS) | protect;
			switch (level)
			{
			case 0: ++protectCount; break;
			case 1: protectCount += 1 << 9; break;
			case 2:
				if (!state->Use1GiB)
					printf("FreeLUT WARN: Range table %p entry %hu has a 1GiB range without 1GiB support\n", rangeTable, i);
				protectCount += 1 << 18;
				break;
			default:
				printf("FreeLUT WARN: Range table %p entry %hu has a range on level %hhu\n", rangeTable, i, level);
				break;
			}
			break;
		}
		default: break;
		}
	}
	return protectCount;
}

static size_t FreeLUTFillUsedRecursive(struct FreeLUTState* state, uintptr_t* pageTable, uintptr_t* rangeTable, uintptr_t firstPage, uintptr_t lastPage, uint32_t flags, uint8_t level)
{
	uint8_t minLevel = 0;
	switch (flags & VMM_PAGE_SIZE_BITS)
	{
	case VMM_PAGE_SIZE_4KiB: minLevel = 0; break;
	case VMM_PAGE_SIZE_2MiB: minLevel = 1; break;
	case VMM_PAGE_SIZE_1GiB: minLevel = 2; break;
	}

	const uint8_t shift      = 9 * level;
	uint16_t      firstEntry = (firstPage >> shift) & 511;
	uint16_t      lastEntry  = (lastPage >> shift) & 511;
	size_t        fillCount  = 0;
	if (level == minLevel)
	{
		uintptr_t pageValue = 0;
		switch (flags & VMM_PAGE_PROTECT_BITS)
		{
		case VMM_PAGE_PROTECT_READ_WRITE: pageValue |= PAGE_TABLE_PROTECT_READ_WRITE; break;
		case VMM_PAGE_PROTECT_READ_ONLY: pageValue |= PAGE_TABLE_PROTECT_READ_ONLY; break;
		case VMM_PAGE_PROTECT_READ_WRITE_EXECUTE: pageValue |= PAGE_TABLE_PROTECT_READ_WRITE_EXECUTE; break;
		case VMM_PAGE_PROTECT_READ_EXECUTE: pageValue |= PAGE_TABLE_PROTECT_READ_EXECUTE; break;
		default: pageValue |= PAGE_TABLE_PROTECT_READ_WRITE; break;
		}
		switch (level)
		{
		case 0: pageValue |= PAGE_TABLE_4KIB_PAGE; break;
		case 1: pageValue |= PAGE_TABLE_2MIB_PAGE; break;
		case 2: pageValue |= PAGE_TABLE_1GIB_PAGE; break;
		}
		uintptr_t rangeValue = 0;
		switch (flags & VMM_PAGE_OPTION_BITS)
		{
		case VMM_PAGE_AUTO_COMMIT: rangeValue |= FREELUT_RANGE_AUTO_COMMIT; break;
		default: rangeValue |= FREELUT_RANGE_UNMAPPED; break;
		}
		for (uint16_t i = firstEntry; i <= lastEntry; ++i)
		{
			pageTable[i]  = pageValue;
			rangeTable[i] = rangeValue;
		}
		fillCount += (1 + lastEntry - firstEntry) << shift;
	}
	else
	{
		for (uint16_t i = firstEntry; i <= lastEntry; ++i)
		{
			uintptr_t rangeEntry = rangeTable[i];
			switch (rangeEntry & FREELUT_RANGE_BITS)
			{
			case FREELUT_RANGE_FREE:
			{
				void*     tablePages    = PMMAlloc(2);
				uintptr_t newPageTable  = 0;
				uintptr_t newRangeTable = 0;
				if (!tablePages)
				{
					newPageTable  = (uintptr_t) PMMAlloc(1);
					newRangeTable = (uintptr_t) PMMAlloc(1);
					if (!newPageTable ||
						!newRangeTable)
					{
						printf("FreeLUT CRITICAL: Failed to allocate pages for table level %hhu\n", level);
						KernelPanic();
					}
				}
				else
				{
					newPageTable  = (uintptr_t) tablePages;
					newRangeTable = (uintptr_t) tablePages + 0x1000;
				}
				state->Stats.Footprint += 2;
				memset((void*) newPageTable, 0, 0x1000);
				memset((void*) newRangeTable, 0, 0x1000);
				pageTable[i]  = newPageTable | PAGE_TABLE_PROTECT_READ_WRITE_EXECUTE | PAGE_TABLE_PRESENT;
				rangeTable[i] = newRangeTable | FREELUT_RANGE_TBL_PTR;
				rangeEntry    = rangeTable[i];
				[[fallthrough]];
			}
			case FREELUT_RANGE_TBL_PTR:
				fillCount += FreeLUTFillUsedRecursive(
					state,
					(uintptr_t*) (pageTable[i] & PAGE_TABLE_4KIB_ADDRESS),
					(uintptr_t*) (rangeEntry & FREELUT_RANGE_4KIB_ADDRESS),
					i == firstEntry ? firstPage - (i << shift) : 0,
					i == lastEntry ? lastPage - (i << shift) : (1UL << shift) - 1,
					flags,
					level - 1);
				break;
			default:
				break;
			}
		}
	}
	return fillCount;
}

static size_t FreeLUTFillFree(struct FreeLUTState* state, struct FreeLUTEntry* entry)
{
	uintptr_t* firstPageTable  = state->PageTableRoot;
	uintptr_t* lastPageTable   = state->PageTableRoot;
	uintptr_t* firstRangeTable = state->RangeTableRoot;
	uintptr_t* lastRangeTable  = state->RangeTableRoot;

	uintptr_t firstPage = entry->Start;
	uintptr_t lastPage  = firstPage + entry->Count - 1;
	size_t    freeCount = 0;
	for (uint8_t level = state->Levels; level-- > 0;)
	{
		const uint8_t shift         = 9 * level;
		uint16_t      firstEntry    = (firstPage >> shift) & 511;
		uint16_t      lastEntry     = (lastPage >> shift) & 511;
		uintptr_t     remMask       = ~0UL >> (64 - shift);
		uintptr_t     remFirstEntry = firstPage & remMask;
		uintptr_t     remLastEntry  = lastPage & remMask;

		int16_t layerFillStart = remFirstEntry == 0 ? firstEntry : firstEntry + 1;
		int16_t layerFillEnd   = remFirstEntry == remMask ? lastEntry : lastEntry - 1;

		if (firstRangeTable == lastRangeTable)
		{
			if (!firstRangeTable)
				break;

			for (int16_t i = layerFillStart; i <= layerFillEnd; ++i)
			{
				uintptr_t rangeEntry = firstRangeTable[i];
				if ((rangeEntry & FREELUT_RANGE_BITS) == FREELUT_RANGE_TBL_PTR)
				{
					FreeLUTFreeRecursive(state, (uintptr_t*) (firstPageTable[i] & PAGE_TABLE_4KIB_ADDRESS), (uintptr_t*) (rangeEntry & FREELUT_RANGE_4KIB_ADDRESS), level - 1);
					freeCount += 1 << shift;
				}
				firstPageTable[i]  = PAGE_TABLE_FREE;
				firstRangeTable[i] = ((uintptr_t) entry & FREELUT_RANGE_32B_ADDRESS) | FREELUT_RANGE_FREE;
			}
		}
		else
		{
			if (firstRangeTable)
			{
				for (int16_t i = layerFillStart; i < 512; ++i)
				{
					uintptr_t rangeEntry = firstRangeTable[i];
					if ((rangeEntry & FREELUT_RANGE_BITS) == FREELUT_RANGE_TBL_PTR)
					{
						FreeLUTFreeRecursive(state, (uintptr_t*) (firstPageTable[i] & PAGE_TABLE_4KIB_ADDRESS), (uintptr_t*) (rangeEntry & FREELUT_RANGE_4KIB_ADDRESS), level - 1);
						freeCount += 1 << shift;
					}
					firstPageTable[i]  = PAGE_TABLE_FREE;
					firstRangeTable[i] = ((uintptr_t) entry & FREELUT_RANGE_32B_ADDRESS) | FREELUT_RANGE_FREE;
				}
			}
			if (lastRangeTable)
			{
				for (int16_t i = 0; i < layerFillEnd; ++i)
				{
					uintptr_t rangeEntry = lastRangeTable[i];
					if ((rangeEntry & FREELUT_RANGE_BITS) == FREELUT_RANGE_TBL_PTR)
					{
						FreeLUTFreeRecursive(state, (uintptr_t*) (lastPageTable[i] & PAGE_TABLE_4KIB_ADDRESS), (uintptr_t*) (rangeEntry & FREELUT_RANGE_4KIB_ADDRESS), level - 1);
						freeCount += 1 << shift;
					}
					lastPageTable[i]  = PAGE_TABLE_FREE;
					lastRangeTable[i] = ((uintptr_t) entry & FREELUT_RANGE_32B_ADDRESS) | FREELUT_RANGE_FREE;
				}
			}
		}

		if (firstRangeTable)
		{
			if (layerFillStart == firstEntry)
			{
				firstPageTable  = nullptr;
				firstRangeTable = nullptr;
			}
			else
			{
				uintptr_t firstRangeEntry = firstRangeTable[firstEntry];
				switch (firstRangeEntry & FREELUT_RANGE_BITS)
				{
				case FREELUT_RANGE_FREE:
					firstPageTable[firstEntry]  = PAGE_TABLE_FREE;
					firstRangeTable[firstEntry] = ((uintptr_t) entry & FREELUT_RANGE_32B_ADDRESS) | FREELUT_RANGE_FREE;
					firstPageTable              = nullptr;
					firstRangeTable             = nullptr;
					break;
				case FREELUT_RANGE_TBL_PTR:
					firstPageTable  = (uintptr_t*) (firstPageTable[firstEntry] & PAGE_TABLE_4KIB_ADDRESS);
					firstRangeTable = (uintptr_t*) (firstRangeTable[firstEntry] & FREELUT_RANGE_4KIB_ADDRESS);
					break;
				case FREELUT_RANGE_MAPPED:
					if (!FreeLUTFreeMappedRange(state, firstPageTable[firstEntry], level))
						printf("FreeLUT WARN: Could not free mapped range in rangle table %p entry %hu on level %hhu", firstRangeTable, firstEntry, level);
					firstPageTable[firstEntry]  = PAGE_TABLE_FREE;
					firstRangeTable[firstEntry] = ((uintptr_t) entry & FREELUT_RANGE_32B_ADDRESS) | FREELUT_RANGE_FREE;
					firstPageTable              = nullptr;
					firstRangeTable             = nullptr;
					freeCount                  += 1 << shift;
					break;
				case FREELUT_RANGE_MAPPED_TO_DISK:
					if (!FreeLUTFreeMappedToDiskRange(state, firstRangeTable[firstEntry], level))
						printf("FreeLUT WARN: Could not free mapped to disk range in rangle table %p entry %hu on level %hhu", firstRangeTable, firstEntry, level);
					firstPageTable[firstEntry]  = PAGE_TABLE_FREE;
					firstRangeTable[firstEntry] = ((uintptr_t) entry & FREELUT_RANGE_32B_ADDRESS) | FREELUT_RANGE_FREE;
					firstPageTable              = nullptr;
					firstRangeTable             = nullptr;
					freeCount                  += 1 << shift;
					break;
				case FREELUT_RANGE_UNMAPPED:
				case FREELUT_RANGE_AUTO_COMMIT:
					freeCount += 1 << shift;
					[[fallthrough]];
				default:
					firstPageTable[firstEntry]  = PAGE_TABLE_FREE;
					firstRangeTable[firstEntry] = ((uintptr_t) entry & FREELUT_RANGE_32B_ADDRESS) | FREELUT_RANGE_FREE;
					firstPageTable              = nullptr;
					firstRangeTable             = nullptr;
					break;
				}
			}
		}
		if (lastRangeTable)
		{
			if (layerFillStart == lastEntry)
			{
				lastPageTable  = nullptr;
				lastRangeTable = nullptr;
			}
			else
			{
				uintptr_t lastRangeEntry = lastRangeTable[lastEntry];
				switch (lastRangeEntry & FREELUT_RANGE_BITS)
				{
				case FREELUT_RANGE_FREE:
					lastPageTable[lastEntry]  = PAGE_TABLE_FREE;
					lastRangeTable[lastEntry] = ((uintptr_t) entry & FREELUT_RANGE_32B_ADDRESS) | FREELUT_RANGE_FREE;
					lastPageTable             = nullptr;
					lastRangeTable            = nullptr;
					break;
				case FREELUT_RANGE_TBL_PTR:
					lastPageTable  = (uintptr_t*) (lastPageTable[lastEntry] & PAGE_TABLE_4KIB_ADDRESS);
					lastRangeTable = (uintptr_t*) (lastRangeTable[lastEntry] & FREELUT_RANGE_4KIB_ADDRESS);
					break;
				case FREELUT_RANGE_MAPPED:
					if (!FreeLUTFreeMappedRange(state, lastPageTable[lastEntry], level))
						printf("FreeLUT WARN: Could not free mapped range in rangle table %p entry %hu on level %hhu", lastRangeTable, lastEntry, level);
					lastPageTable[lastEntry]  = PAGE_TABLE_FREE;
					lastRangeTable[lastEntry] = ((uintptr_t) entry & FREELUT_RANGE_32B_ADDRESS) | FREELUT_RANGE_FREE;
					lastPageTable             = nullptr;
					lastRangeTable            = nullptr;
					freeCount                += 1 << shift;
					break;
				case FREELUT_RANGE_MAPPED_TO_DISK:
					if (!FreeLUTFreeMappedToDiskRange(state, lastRangeTable[lastEntry], level))
						printf("FreeLUT WARN: Could not free mapped to disk range in rangle table %p entry %hu on level %hhu", lastRangeTable, lastEntry, level);
					lastPageTable[lastEntry]  = PAGE_TABLE_FREE;
					lastRangeTable[lastEntry] = ((uintptr_t) entry & FREELUT_RANGE_32B_ADDRESS) | FREELUT_RANGE_FREE;
					lastPageTable             = nullptr;
					lastRangeTable            = nullptr;
					freeCount                += 1 << shift;
					break;
				case FREELUT_RANGE_UNMAPPED:
				case FREELUT_RANGE_AUTO_COMMIT:
					freeCount += 1 << shift;
					[[fallthrough]];
				default:
					lastPageTable[lastEntry]  = PAGE_TABLE_FREE;
					lastRangeTable[lastEntry] = ((uintptr_t) entry & FREELUT_RANGE_32B_ADDRESS) | FREELUT_RANGE_FREE;
					lastPageTable             = nullptr;
					lastRangeTable            = nullptr;
					break;
				}
			}
		}
	}
	return freeCount;
}

static bool FreeLUTMapPages(struct FreeLUTState* state, uintptr_t page, uintptr_t physicalAddress)
{
	uintptr_t* pageTable  = state->PageTableRoot;
	uintptr_t* rangeTable = state->RangeTableRoot;
	for (uint8_t i = state->Levels; i-- > 0;)
	{
		uint16_t  entry      = (page >> (9 * i)) & 511;
		uintptr_t rangeEntry = rangeTable[entry];
		switch (rangeEntry & FREELUT_RANGE_BITS)
		{
		case FREELUT_RANGE_TBL_PTR:
			pageTable  = (uintptr_t*) (pageTable[entry] & PAGE_TABLE_4KIB_ADDRESS);
			rangeTable = (uintptr_t*) (rangeTable[entry] & FREELUT_RANGE_4KIB_ADDRESS);
			break;
		case FREELUT_RANGE_MAPPED:
			switch (i)
			{
			case 0:
				pageTable[entry] = (pageTable[entry] & ~PAGE_TABLE_4KIB_ADDRESS) | (physicalAddress & PAGE_TABLE_4KIB_ADDRESS);
				break;
			case 1:
				pageTable[entry] = (pageTable[entry] & ~PAGE_TABLE_2MIB_ADDRESS) | (physicalAddress & PAGE_TABLE_2MIB_ADDRESS);
				break;
			case 2:
				pageTable[entry] = (pageTable[entry] & ~PAGE_TABLE_1GIB_ADDRESS) | (physicalAddress & PAGE_TABLE_1GIB_ADDRESS);
				break;
			}
			return true;
		case FREELUT_RANGE_MAPPED_TO_DISK:
			switch (i)
			{
			case 0: --state->Stats.PagesMappedToDisk; break;
			case 1: state->Stats.PagesMappedToDisk -= 1 << 9; break;
			case 2: state->Stats.PagesMappedToDisk -= 1 << 18; break;
			}
			[[fallthrough]];
		case FREELUT_RANGE_UNMAPPED:
		case FREELUT_RANGE_AUTO_COMMIT:
			switch (i)
			{
			case 0:
				pageTable[entry] = (pageTable[entry] & ~PAGE_TABLE_4KIB_ADDRESS) | (physicalAddress & PAGE_TABLE_4KIB_ADDRESS) | PAGE_TABLE_PRESENT;
				++state->Stats.PagesMapped;
				break;
			case 1:
				pageTable[entry]          = (pageTable[entry] & ~PAGE_TABLE_2MIB_ADDRESS) | (physicalAddress & PAGE_TABLE_2MIB_ADDRESS) | PAGE_TABLE_PRESENT;
				state->Stats.PagesMapped += 1 << 9;
				break;
			case 2:
				if (!state->Use1GiB)
				{
					printf("FreeLUT WARN: Range table %p entry %hu has a 1GiB unmapped range without 1GiB support\n", rangeTable, i);
					return false;
				}
				pageTable[entry]          = (pageTable[entry] & ~PAGE_TABLE_1GIB_ADDRESS) | (physicalAddress & PAGE_TABLE_1GIB_ADDRESS) | PAGE_TABLE_PRESENT;
				state->Stats.PagesMapped += 1 << 18;
				break;
			default:
				printf("FreeLUT WARN: Range table %p entry %hu has an unmapped range on level %hhu\n", rangeTable, entry, i);
				break;
			}
			return true;
		default: return false;
		}
	}
	return false;
}

static size_t FreeLUTMapLinear(struct FreeLUTState* state, uintptr_t firstPage, uintptr_t lastPage, uintptr_t physicalAddress)
{
	return FreeLUTMapLinearRecursive(state, state->PageTableRoot, state->RangeTableRoot, firstPage, lastPage, physicalAddress, state->Levels - 1);
}

static size_t FreeLUTProtect(struct FreeLUTState* state, uintptr_t firstPage, uintptr_t lastPage, uint32_t protect)
{
	uintptr_t prot = 0;
	switch (protect)
	{
	case VMM_PAGE_PROTECT_READ_WRITE: prot = PAGE_TABLE_PROTECT_READ_WRITE; break;
	case VMM_PAGE_PROTECT_READ_ONLY: prot = PAGE_TABLE_PROTECT_READ_ONLY; break;
	case VMM_PAGE_PROTECT_READ_WRITE_EXECUTE: prot = PAGE_TABLE_PROTECT_READ_WRITE_EXECUTE; break;
	case VMM_PAGE_PROTECT_READ_EXECUTE: prot = PAGE_TABLE_PROTECT_READ_EXECUTE; break;
	default: prot = PAGE_TABLE_PROTECT_READ_WRITE; break;
	}
	return FreeLUTProtectRecursive(state, state->PageTableRoot, state->RangeTableRoot, firstPage, lastPage, prot, state->Levels - 1);
}

static size_t FreeLUTFillUsed(struct FreeLUTState* state, uintptr_t firstPage, uintptr_t lastPage, uint32_t flags)
{
	return FreeLUTFillUsedRecursive(state, state->PageTableRoot, state->RangeTableRoot, firstPage, lastPage, flags, state->Levels - 1);
}

static uintptr_t FreeLUTGetPhysicalAddress(struct FreeLUTState* state, uintptr_t page)
{
	uintptr_t* pageTable  = state->PageTableRoot;
	uintptr_t* rangeTable = state->RangeTableRoot;
	for (uint8_t i = state->Levels; i-- > 0;)
	{
		uint16_t  entry      = (page >> (9 * i)) & 511;
		uintptr_t rangeEntry = rangeTable[entry];
		switch (rangeEntry & FREELUT_RANGE_BITS)
		{
		case FREELUT_RANGE_TBL_PTR:
			pageTable  = (uintptr_t*) (pageTable[entry] & PAGE_TABLE_4KIB_ADDRESS);
			rangeTable = (uintptr_t*) (rangeTable[entry] & FREELUT_RANGE_4KIB_ADDRESS);
			break;
		case FREELUT_RANGE_MAPPED:
			switch (i)
			{
			case 0: return pageTable[entry] & PAGE_TABLE_4KIB_ADDRESS;
			case 1: return pageTable[entry] & PAGE_TABLE_2MIB_ADDRESS;
			case 2: return pageTable[entry] & PAGE_TABLE_1GIB_ADDRESS;
			default: return 0;
			}
		default: return 0;
		}
	}
	return 0;
}

static uint8_t FreeLUTPageGetFree(struct FreeLUTPage* page)
{
	if (page->Bitmap[0] == 0)
		return page->Bitmap[1] == 0 ? 128 : __builtin_clzll(page->Bitmap[1]);
	return __builtin_clzll(page->Bitmap[0]);
}

static struct FreeLUTEntry* FreeLUTPageAlloc(struct FreeLUTState* state)
{
	if (!state->FirstPage)
	{
		state->FirstPage = (struct FreeLUTPage*) PMMAlloc(1);
		state->LastPage  = state->FirstPage;
		memset(state->FirstPage, 0, 0x1000);
		state->FirstPage->Bitmap[0] = ~0UL;
		state->FirstPage->Bitmap[1] = ~0UL >> 1;
		++state->Stats.Footprint;
	}
	struct FreeLUTPage* page  = state->FirstPage;
	uint8_t             entry = FreeLUTPageGetFree(page);
	if (entry == 128)
	{
		page = (struct FreeLUTPage*) PMMAlloc(1);
		memset(page, 0, 0x1000);
		page->Bitmap[0]        = ~0UL;
		page->Bitmap[1]        = ~0UL >> 1;
		page->Next             = state->FirstPage;
		state->FirstPage->Prev = page;
		state->FirstPage       = page;
		entry                  = 0;
		++state->Stats.Footprint;
	}

	page->Bitmap[entry >> 7] &= ~(1UL << (entry & 127));
	struct FreeLUTEntry* entr = &page->Entries[entry];

	if ((page->Bitmap[0] | page->Bitmap[1]) == 0)
	{
		if (page->Prev)
			page->Prev->Next = page->Next;
		if (page->Next)
			page->Next->Prev = page->Prev;
		if (state->LastPage != page)
		{
			state->LastPage->Next = page;
			page->Prev            = state->LastPage;
		}
		page->Next      = nullptr;
		state->LastPage = page;
	}
	return entr;
}

static void FreeLUTPageFree(struct FreeLUTState* state, struct FreeLUTEntry* entry)
{
	struct FreeLUTPage* page = (struct FreeLUTPage*) ((uintptr_t) entry & ~0xFFFUL);
	uint8_t             entr = (uint8_t) (entry - page->Entries);
	page->Bitmap[entr >> 7] |= 1UL << (entr & 127);

	if (state->LastPage == page)
		state->LastPage = page->Prev;
	if (state->FirstPage == page)
		state->FirstPage = page->Next;
	if (page->Prev)
		page->Prev->Next = page->Next;
	if (page->Next)
		page->Next->Prev = page->Prev;

	if (page->Bitmap[0] != ~0UL && page->Bitmap[1] != (~0UL >> 1))
	{
		page->Prev = nullptr;
		page->Next = state->FirstPage;
		if (state->FirstPage)
			state->FirstPage->Prev = page;
		state->FirstPage = page;
	}
	else
	{
		PMMFree(page, 1);
		--state->Stats.Footprint;
	}
}

static struct FreeLUTEntry* FreeLUTNewRange(struct FreeLUTState* state, uintptr_t firstPage, uintptr_t lastPage)
{
	struct FreeLUTEntry* entry = FreeLUTPageAlloc(state);
	entry->Start               = firstPage;
	entry->Count               = 1 + lastPage - firstPage;
	uint8_t index              = FreeLUTGetFloorIndex(entry->Count);
	if (state->LUT[index])
	{
		struct FreeLUTEntry* other = state->LUT[index];
		if (other->Prev)
			other->Prev->Next = entry;
		entry->Next = other;
		entry->Prev = other->Prev;
		other->Prev = entry;
		for (uint8_t i = index + 1; i-- > 0;)
		{
			if (state->LUT[i] != other)
				break;
			state->LUT[i] = entry;
		}
		return entry;
	}

	for (uint8_t i = index + 1; i-- > 0;)
	{
		if (state->LUT[i])
			break;
		state->LUT[i] = entry;
	}
	if (state->Last)
		state->Last->Next = entry;
	entry->Prev = state->Last;
	entry->Next = nullptr;
	state->Last = entry;
	return entry;
}

static void FreeLUTEraseRange(struct FreeLUTState* state, struct FreeLUTEntry* entry)
{
	if (state->Last == entry)
		state->Last = entry->Prev;
	uint8_t index = FreeLUTGetFloorIndex(entry->Count);
	for (uint8_t i = index + 1; i-- > 0;)
	{
		if (state->LUT[i] != entry)
			break;
		state->LUT[i] = entry->Next;
	}
	if (entry->Prev)
		entry->Prev->Next = entry->Next;
	if (entry->Next)
		entry->Next->Prev = entry->Prev;
	entry->Prev = nullptr;
	entry->Next = nullptr;
	FreeLUTPageFree(state, entry);
}

static struct FreeLUTEntry* FreeLUTGetFreeRange(struct FreeLUTState* state, uint64_t count)
{
	uint8_t index = FreeLUTGetCeilIndex(count);
	if (state->LUT[index])
		return state->LUT[index];

	if (index == 0 ||
		FreeLUTGetValue(index) == count)
		return nullptr;

	struct FreeLUTEntry* cur = state->LUT[index - 1];
	while (cur && cur->Count < count)
		cur = cur->Next;
	return cur;
}

static struct FreeLUTEntry* FreeLUTGetFreeRangeAt(struct FreeLUTState* state, uintptr_t page, uint64_t count)
{
	uintptr_t* rangeTable = state->RangeTableRoot;
	for (uint8_t i = state->Levels; rangeTable && i-- > 0;)
	{
		uintptr_t rangeEntry = rangeTable[(page >> (9 * i)) & 511];
		switch (rangeEntry & FREELUT_RANGE_BITS)
		{
		case FREELUT_RANGE_FREE:
		{
			struct FreeLUTEntry* entry = (struct FreeLUTEntry*) (rangeEntry & FREELUT_RANGE_32B_ADDRESS);
			return entry && (entry->Start + entry->Count >= page + count) ? entry : nullptr;
		}
		case FREELUT_RANGE_TBL_PTR:
			rangeTable = (uintptr_t*) (rangeEntry & FREELUT_RANGE_4KIB_ADDRESS);
			break;
		default:
			return nullptr;
		}
	}
	return nullptr;
}

static struct FreeLUTEntry* FreeLUTGetFreeRangeAligned(struct FreeLUTState* state, uint64_t count, uint8_t alignment)
{
	uint8_t              index = FreeLUTGetFloorIndex(count);
	struct FreeLUTEntry* cur   = state->LUT[index];

	const uintptr_t alignmentVal  = 1UL << alignment;
	const uintptr_t alignmentMask = alignmentVal - 1;
	const uintptr_t temp          = ~alignmentMask;
	while (cur && ((cur->Start + alignmentMask) & temp) > (cur->Start + cur->Count))
		cur = cur->Next;
	return cur;
}

void* FreeLUTVMMCreate(void)
{
	struct FreeLUTState* state = (struct FreeLUTState*) PMMAlloc(3);
	if (!state)
		return nullptr;

	memset(state, 0, 0x3000);
	state->Stats = (struct VMMMemoryStats) {
		.Footprint         = 1,
		.PagesAllocated    = 0,
		.PagesMapped       = 0,
		.PagesMappedToDisk = 0,
		.AllocCalls        = 0,
		.FreeCalls         = 0,
		.ProtectCalls      = 0,
		.MapCalls          = 0
	};
	// TODO(MarcasRealAccount): Determine max levels and 1GiB support
	state->Levels              = 4;
	state->Use1GiB             = false;
	state->PageTableRoot       = (uintptr_t*) state + 0x1000;
	state->RangeTableRoot      = (uintptr_t*) state + 0x2000;
	struct FreeLUTEntry* entry = FreeLUTNewRange(state, 1, (1UL << (9 * state->Levels)) - 2);
	FreeLUTFillFree(state, entry);
	return state;
}

void FreeLUTVMMDestroy(void* vmm)
{
	struct FreeLUTState* state = (struct FreeLUTState*) vmm;

	FreeLUTFreeRecursive(state, state->PageTableRoot, state->RangeTableRoot, state->Levels - 1);
	struct FreeLUTPage* curPage = state->FirstPage;
	while (curPage)
	{
		struct FreeLUTPage* nextPage = curPage->Next;
		PMMFree(curPage, 1);
		curPage = nextPage;
	}
	PMMFree(state, 1);
}

void FreeLUTVMMGetMemoryStats(void* vmm, struct VMMMemoryStats* stats)
{
	struct FreeLUTState* state = (struct FreeLUTState*) vmm;
	*stats                     = state->Stats;
}

void* FreeLUTVMMAlloc(void* vmm, size_t count, uint8_t alignment, uint32_t flags)
{
	if (count == 0)
		return nullptr;
	struct FreeLUTState* state = (struct FreeLUTState*) vmm;
	if ((flags & VMM_PAGE_SIZE_BITS) == VMM_PAGE_SIZE_1GiB && !state->Use1GiB)
		flags = (flags & ~VMM_PAGE_SIZE_BITS) | VMM_PAGE_SIZE_2MiB;

	uintptr_t alignmentVal  = 1UL << (alignment - 12);
	uintptr_t alignmentMask = alignmentVal - 1;

	struct FreeLUTEntry* entry = FreeLUTGetFreeRange(state, count + alignmentMask);
	if (!entry)
	{
		entry = FreeLUTGetFreeRangeAligned(state, count, alignment);
		if (!entry)
			return nullptr;
	}

	++state->Stats.AllocCalls;
	state->Stats.PagesAllocated += count;
	uintptr_t entryPage          = entry->Start;
	uintptr_t lastRangePage      = entryPage + entry->Count - 1;
	uintptr_t firstPage          = (entryPage + alignmentMask) & ~alignmentMask;
	uintptr_t lastPage           = firstPage + count - 1;

	FreeLUTEraseRange(state, entry);
	FreeLUTFillUsed(state, firstPage, lastPage, flags);
	if (entryPage != firstPage)
	{
		struct FreeLUTEntry* firstEntry = FreeLUTNewRange(state, entryPage, firstPage - 1);
		FreeLUTFillFree(state, firstEntry);
	}
	if (lastPage != lastRangePage)
	{
		struct FreeLUTEntry* lastEntry = FreeLUTNewRange(state, lastPage + 1, lastRangePage);
		FreeLUTFillFree(state, lastEntry);
	}
	return (void*) (firstPage << 12);
}

void* FreeLUTVMMAllocAt(void* vmm, void* virtualAddress, size_t count, uint32_t flags)
{
	if (count == 0)
		return nullptr;

	uintptr_t firstPage = (uintptr_t) virtualAddress >> 12;

	struct FreeLUTState* state = (struct FreeLUTState*) vmm;
	if ((flags & VMM_PAGE_SIZE_BITS) == VMM_PAGE_SIZE_1GiB && !state->Use1GiB)
		flags = (flags & ~VMM_PAGE_SIZE_BITS) | VMM_PAGE_SIZE_2MiB;
	struct FreeLUTEntry* entry = FreeLUTGetFreeRangeAt(state, firstPage, count);
	if (!entry)
		return nullptr;

	++state->Stats.AllocCalls;
	state->Stats.PagesAllocated += count;
	uintptr_t entryPage          = entry->Start;
	uintptr_t lastRangePage      = entryPage + entry->Count - 1;
	uintptr_t lastPage           = firstPage + count - 1;

	FreeLUTEraseRange(state, entry);
	FreeLUTFillUsed(state, firstPage, lastPage, flags);
	if (entryPage != firstPage)
	{
		struct FreeLUTEntry* firstEntry = FreeLUTNewRange(state, entryPage, firstPage - 1);
		FreeLUTFillFree(state, firstEntry);
	}
	if (lastPage != lastRangePage)
	{
		struct FreeLUTEntry* lastEntry = FreeLUTNewRange(state, lastPage + 1, lastRangePage);
		FreeLUTFillFree(state, lastEntry);
	}
	return (void*) (firstPage << 12);
}

void FreeLUTVMMFree(void* vmm, void* virtualAddress, size_t count)
{
	if (!virtualAddress ||
		count == 0)
		return;

	uintptr_t firstPage = (uintptr_t) virtualAddress >> 12;

	struct FreeLUTState* state = (struct FreeLUTState*) vmm;
	if (FreeLUTGetFreeRangeAt(state, firstPage, 1))
		return;

	++state->Stats.FreeCalls;
	state->Stats.PagesAllocated -= count;

	uintptr_t bottomPage = firstPage;
	size_t    totalCount = count;

	struct FreeLUTEntry* entryBelow = FreeLUTGetFreeRangeAt(state, firstPage - 1, 1);
	struct FreeLUTEntry* entryAbove = FreeLUTGetFreeRangeAt(state, firstPage + count, 1);
	if (entryBelow)
	{
		bottomPage  = entryBelow->Start;
		totalCount += entryBelow->Count;
		FreeLUTEraseRange(state, entryBelow);
	}
	if (entryAbove)
	{
		totalCount += entryAbove->Count;
		FreeLUTEraseRange(state, entryAbove);
	}
	struct FreeLUTEntry* entry = FreeLUTNewRange(state, firstPage, firstPage + totalCount - 1);
	FreeLUTFillFree(state, entry);
}

void FreeLUTVMMProtect(void* vmm, void* virtualAddress, size_t count, uint32_t protect)
{
	struct FreeLUTState* state = (struct FreeLUTState*) vmm;

	++state->Stats.ProtectCalls;
	uintptr_t firstPage = (uintptr_t) virtualAddress >> 12;
	uintptr_t lastPage  = firstPage + count - 1;
	FreeLUTProtect(state, firstPage, lastPage, protect);
}

void FreeLUTVMMMap(void* vmm, void* virtualAddress, void* physicalAddress)
{
	struct FreeLUTState* state = (struct FreeLUTState*) vmm;
	++state->Stats.MapCalls;
	FreeLUTMapPages(state, (uintptr_t) virtualAddress >> 12, (uintptr_t) physicalAddress);
}

void FreeLUTVMMMapLinear(void* vmm, void* virtualAddress, void* physicalAddress, size_t count)
{
	struct FreeLUTState* state     = (struct FreeLUTState*) vmm;
	uintptr_t            firstPage = (uintptr_t) virtualAddress >> 12;
	uintptr_t            lastPage  = firstPage + count - 1;
	++state->Stats.MapCalls;
	FreeLUTMapLinear(state, firstPage, lastPage, (uintptr_t) physicalAddress);
}

void* FreeLUTVMMTranslate(void* vmm, void* virtualAddress)
{
	struct FreeLUTState* state = (struct FreeLUTState*) vmm;
	return (void*) FreeLUTGetPhysicalAddress(state, (uintptr_t) virtualAddress >> 12);
}

void* FreeLUTVMMGetRootTable(void* vmm, uint8_t* levels, bool* use1GiB)
{
	struct FreeLUTState* state = (struct FreeLUTState*) vmm;
	if (levels)
		*levels = state->Levels;
	if (use1GiB)
		*use1GiB = state->Use1GiB;
	return state->PageTableRoot;
}