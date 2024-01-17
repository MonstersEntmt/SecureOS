#include "DebugCon.h"
#define PMM_USE_FREELIST_LUT 1

// TODO(MarcasRealAccount): Implement allocator selection as a runtime option through the commandline
#if PMM_USE_FREELIST_LUT

	#include "PMM.h"

	#include <string.h>

struct PMMFreeHeader
{
	int64_t               Count;
	struct PMMFreeHeader* Prev;
	struct PMMFreeHeader* Next;
};

struct PMMState
{
	struct PMMMemoryStats Stats;

	size_t                    MemoryMapCount;
	struct PMMMemoryMapEntry* MemoryMap;

	uint64_t*             Bitmap;
	struct PMMFreeHeader* Last;
	struct PMMFreeHeader* LUT[255];
};

struct PMMState* g_PMM;

static uint64_t PMMGetLUTValue(uint8_t index)
{
	if (index < 192)
		return (uint64_t) index + 1;
	return (1UL << (index - 191)) + 192;
}

static uint8_t PMMGetLUTIndex(uint64_t value)
{
	if (value < 193)
		return (uint8_t) value - 1;
	return (254 - __builtin_clzll(value - 192));
}

static uint8_t PMMGetLUTCeilIndex(uint64_t value)
{
	if (value < 193)
		return (uint8_t) value - 1;
	return (255 - __builtin_clzll(value - 193));
}

static bool PMMBitmapGetEntry(uint64_t page)
{
	return (g_PMM->Bitmap[page >> 6] >> (page & 63)) & 1;
}

static void PMMBitmapSetEntry(uint64_t page, bool value)
{
	if (value)
		g_PMM->Bitmap[page >> 6] |= 1UL << (page & 63);
	else
		g_PMM->Bitmap[page >> 6] &= ~(1UL << (page & 63));
}

static void PMMBitmapSetRange(uint64_t firstPage, uint64_t lastPage, bool value)
{
	uint64_t firstQword = firstPage >> 6;
	uint64_t lastQword  = lastPage >> 6;

	uint64_t lowMask  = ~0UL << (firstPage & 63);
	uint64_t highMask = ~0UL >> (63 - (lastPage & 63));
	if (firstQword == lastQword)
	{
		if (value)
			g_PMM->Bitmap[firstQword] |= lowMask & highMask;
		else
			g_PMM->Bitmap[firstQword] &= ~(lowMask & highMask);
	}
	else
	{
		if (value)
		{
			g_PMM->Bitmap[firstQword] |= lowMask;
			g_PMM->Bitmap[lastQword]  |= highMask;
			if (lastQword > firstQword + 1)
				memset(g_PMM->Bitmap + firstQword + 1, 0xFF, (lastQword - firstQword - 1) * 8);
		}
		else
		{
			g_PMM->Bitmap[firstQword] &= ~lowMask;
			g_PMM->Bitmap[lastQword]  &= ~highMask;
			if (lastQword > firstQword + 1)
				memset(g_PMM->Bitmap + firstQword + 1, 0x00, (lastQword - firstQword - 1) * 8);
		}
	}
}

static void PMMFillFreePages(uint64_t firstPage, uint64_t lastPage)
{
	struct PMMFreeHeader* firstHeader = (struct PMMFreeHeader*) (firstPage * 4096);
	uint64_t              pageCount   = lastPage - firstPage + 1;
	firstHeader->Count                = pageCount;
	firstHeader->Prev                 = nullptr;
	firstHeader->Next                 = nullptr;
	if (firstPage != lastPage)
	{
		struct PMMFreeHeader* lastHeader = (struct PMMFreeHeader*) (lastPage * 4096);
		lastHeader->Count                = -pageCount;
		lastHeader->Prev                 = nullptr;
		lastHeader->Next                 = nullptr;
	}
}

static struct PMMFreeHeader* PMMGetFirstPage(struct PMMFreeHeader* header)
{
	if (header->Count < 0)
		return (struct PMMFreeHeader*) ((uint8_t*) header + (header->Count + 1) * 4096);
	return header;
}

static void PMMInsertFreeRange(struct PMMFreeHeader* header)
{
	uint8_t index = PMMGetLUTIndex(header->Count);
	if (g_PMM->LUT[index])
	{
		struct PMMFreeHeader* other = g_PMM->LUT[index];
		if (other->Prev)
			other->Prev->Next = header;
		header->Next = other;
		header->Prev = other->Prev;
		other->Prev  = header;
		for (uint8_t i = index + 1; i-- > 0;)
		{
			if (g_PMM->LUT[i] != other)
				break;
			g_PMM->LUT[i] = header;
		}
		return;
	}

	for (uint8_t i = index + 1; i-- > 0;)
	{
		if (g_PMM->LUT[i])
			break;
		g_PMM->LUT[i] = header;
	}
	if (g_PMM->Last)
		g_PMM->Last->Next = header;
	header->Prev = g_PMM->Last;
	header->Next = nullptr;
	g_PMM->Last  = header;
}

static void PMMEraseFreeRange(struct PMMFreeHeader* header)
{
	if (g_PMM->Last == header)
		g_PMM->Last = header->Prev;
	uint8_t index = PMMGetLUTIndex(header->Count);
	for (uint8_t i = index + 1; i-- > 0;)
	{
		if (g_PMM->LUT[i] != header)
			break;
		g_PMM->LUT[i] = header->Next;
	}

	if (header->Prev)
		header->Prev->Next = header->Next;
	if (header->Next)
		header->Next->Prev = header->Prev;
	header->Prev = nullptr;
	header->Next = nullptr;
}

static struct PMMFreeHeader* PMMTakeFreeRange(uint64_t count)
{
	if (count == 1)
	{
		struct PMMFreeHeader* header = g_PMM->LUT[0];
		if (header)
			PMMEraseFreeRange(header);
		return header;
	}

	uint8_t index = PMMGetLUTCeilIndex(count);
	if (g_PMM->LUT[index])
	{
		struct PMMFreeHeader* header = g_PMM->LUT[index];
		PMMEraseFreeRange(header);
		return header;
	}

	if (index == 0 ||
		PMMGetLUTValue(index) == count)
		return nullptr;

	struct PMMFreeHeader* cur = g_PMM->LUT[index - 1];
	while (cur && cur->Count < count)
		cur = cur->Next;
	if (cur)
		PMMEraseFreeRange(cur);
	return cur;
}

static struct PMMFreeHeader* PMMTakeAlignedRange(uint64_t count, uint8_t alignment)
{
	uint8_t               index = PMMGetLUTIndex(count);
	struct PMMFreeHeader* cur   = g_PMM->LUT[index];

	uint64_t alignmentVal  = 1UL << (alignment - 12);
	uint64_t alignmentMask = alignmentVal - 1;
	while (cur && (((uint64_t) cur / 4096 + alignmentMask) & ~alignmentMask) > ((uint64_t) cur / 4096 + cur->Count))
		cur = cur->Next;
	if (cur)
		PMMEraseFreeRange(cur);
	return cur;
}

void PMMInit(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata)
{
	struct PMMMemoryMapEntry tempMemoryMapEntry;

	uint64_t lastUsableAddress = 0;
	uint64_t lastAddress       = 0;
	if (!getter(userdata, entryCount - 1, &tempMemoryMapEntry))
		return; // TODO(MarcasRealAccount): Panic
	lastAddress = tempMemoryMapEntry.Start + tempMemoryMapEntry.Size;
	for (size_t i = entryCount; i-- > 0;)
	{
		if (!getter(userdata, i, &tempMemoryMapEntry) ||
			!(tempMemoryMapEntry.Type & PMMMemoryMapTypeUsable))
			continue;
		lastUsableAddress = tempMemoryMapEntry.Start + tempMemoryMapEntry.Size;
		break;
	}

	size_t pmmRequiredSize = sizeof(struct PMMState) + ((lastUsableAddress + 32767) / 32768); // (32768 = 4 KiB page size * 8 pages per byte)
	pmmRequiredSize        = (pmmRequiredSize + 4095) & ~0xFFFUL;
	size_t pmmAllocatedIn  = ~0UL;
	for (size_t i = 0; i < entryCount; ++i)
	{
		if (!getter(userdata, i, &tempMemoryMapEntry) ||
			!(tempMemoryMapEntry.Type & PMMMemoryMapTypeUsable))
			continue;
		if (tempMemoryMapEntry.Start == 0)
		{
			tempMemoryMapEntry.Start += 4096;
			tempMemoryMapEntry.Size  -= 4096;
		}
		if (tempMemoryMapEntry.Size <= pmmRequiredSize)
			continue;
		pmmAllocatedIn = i;
		g_PMM          = (struct PMMState*) tempMemoryMapEntry.Start;
		break;
	}
	if (pmmAllocatedIn >= entryCount)
		return; // TODO(MarcasRealAccount): Panic

	g_PMM->Stats = (struct PMMMemoryStats) {
		.AllocatorFootprint = pmmRequiredSize,
		.LastUsableAddress  = lastUsableAddress,
		.LastAddress        = lastAddress,
		.PagesFree          = 0
	};
	g_PMM->MemoryMapCount = 0;
	g_PMM->MemoryMap      = nullptr;
	g_PMM->Bitmap         = (uint64_t*) ((uint8_t*) g_PMM + sizeof(struct PMMState));
	g_PMM->Last           = nullptr;
	memset(g_PMM->Bitmap, 0, lastUsableAddress / 32768);
	memset(g_PMM->LUT, 0, sizeof(g_PMM->LUT));

	for (size_t i = 0; i < entryCount; ++i)
	{
		if (!getter(userdata, i, &tempMemoryMapEntry) ||
			tempMemoryMapEntry.Type != PMMMemoryMapTypeUsable)
			continue;

		if (tempMemoryMapEntry.Start == 0)
		{
			tempMemoryMapEntry.Start += 4096;
			tempMemoryMapEntry.Size  -= 4096;
		}
		if (pmmAllocatedIn == i)
		{
			tempMemoryMapEntry.Start += pmmRequiredSize;
			tempMemoryMapEntry.Size  -= pmmRequiredSize;
		}

		PMMFree((void*) tempMemoryMapEntry.Start, tempMemoryMapEntry.Size / 4096);
	}

	g_PMM->MemoryMapCount                 = entryCount + 2;
	g_PMM->MemoryMap                      = PMMAlloc((g_PMM->MemoryMapCount + 4095) / 4096);
	g_PMM->Stats.AllocatorFootprint      += (g_PMM->MemoryMapCount + 4095) & ~0xFFFUL;
	size_t curMemoryMapEntry              = 0;
	g_PMM->MemoryMap[curMemoryMapEntry++] = (struct PMMMemoryMapEntry) {
		.Start = 0,
		.Size  = 4096,
		.Type  = PMMMemoryMapTypeNullGuard
	};
	for (size_t i = 0; i < entryCount; ++i)
	{
		if (!getter(userdata, i, &tempMemoryMapEntry))
			continue;

		if (tempMemoryMapEntry.Start == 0)
		{
			tempMemoryMapEntry.Start += 4096;
			tempMemoryMapEntry.Size  -= 4096;
		}
		if (pmmAllocatedIn == i)
		{
			g_PMM->MemoryMap[curMemoryMapEntry++] = (struct PMMMemoryMapEntry) {
				.Start = tempMemoryMapEntry.Start,
				.Size  = pmmRequiredSize,
				.Type  = PMMMemoryMapTypePMM
			};
			tempMemoryMapEntry.Start += pmmRequiredSize;
			tempMemoryMapEntry.Size  -= pmmRequiredSize;
		}
		if (tempMemoryMapEntry.Size > 0)
		{
			if (tempMemoryMapEntry.Type == PMMMemoryMapTypeUsable)
				tempMemoryMapEntry.Type = PMMMemoryMapTypeTaken;
			g_PMM->MemoryMap[curMemoryMapEntry++] = (struct PMMMemoryMapEntry) {
				.Start = tempMemoryMapEntry.Start,
				.Size  = tempMemoryMapEntry.Size,
				.Type  = tempMemoryMapEntry.Type
			};
		}
	}
}

void PMMReclaim()
{
	for (size_t i = 0; i < g_PMM->MemoryMapCount; ++i)
	{
		struct PMMMemoryMapEntry* entry = &g_PMM->MemoryMap[i];
		if (!(entry->Type & PMMMemoryMapTypeUsable))
			continue;

		PMMFree((void*) entry->Start, entry->Size / 4096);
		entry->Type = PMMMemoryMapTypeTaken;
	}

	size_t                    moveCount = 0;
	struct PMMMemoryMapEntry* pEntry    = &g_PMM->MemoryMap[0];
	for (size_t i = 1; i < g_PMM->MemoryMapCount; ++i)
	{
		struct PMMMemoryMapEntry* entry = &g_PMM->MemoryMap[i];
		if (entry->Type == pEntry->Type &&
			entry->Start == pEntry->Start + pEntry->Size)
		{
			pEntry->Size += entry->Size;
			++moveCount;
		}
		else
		{
			g_PMM->MemoryMap[i - moveCount] = *entry;
			pEntry                          = &g_PMM->MemoryMap[i - moveCount];
		}
	}
	g_PMM->MemoryMapCount -= moveCount;
}

void PMMGetMemoryStats(struct PMMMemoryStats* stats)
{
	if (stats)
		*stats = g_PMM->Stats;
}

size_t PMMGetMemoryMap(const struct PMMMemoryMapEntry** entries)
{
	if (!entries)
		return 0;
	*entries = g_PMM->MemoryMap;
	return g_PMM->MemoryMapCount;
}

void PMMDebugPrint()
{
	DebugCon_WriteString("PMM Memory Map:\n");
	for (size_t i = 0; i < g_PMM->MemoryMapCount; ++i)
	{
		struct PMMMemoryMapEntry* entry   = &g_PMM->MemoryMap[i];
		const char*               typeStr = nullptr;
		switch (entry->Type)
		{
		case PMMMemoryMapTypeInvalid: typeStr = "Invalid"; break;
		case PMMMemoryMapTypeUsable: typeStr = "Usable"; break;
		case PMMMemoryMapTypeReclaimable: typeStr = "Reclaimable"; break;
		case PMMMemoryMapTypeLoaderReclaimable: typeStr = "Loader Reclaimable"; break;
		case PMMMemoryMapTypeTaken: typeStr = "Taken"; break;
		case PMMMemoryMapTypeNullGuard: typeStr = "NullGuard"; break;
		case PMMMemoryMapTypePMM: typeStr = "PMM"; break;
		case PMMMemoryMapTypeKernel: typeStr = "Kernel"; break;
		case PMMMemoryMapTypeModule: typeStr = "Module"; break;
		case PMMMemoryMapTypeReserved: typeStr = "Reserved"; break;
		case PMMMemoryMapTypeNVS: typeStr = "NVS"; break;
		}
		DebugCon_WriteFormatted("%20s: 0x%016lX -> 0x%016lx(%lu)\n", typeStr, entry->Start, entry->Start + entry->Size, entry->Size / 4096);
	}

	DebugCon_WriteString("PMM Free List:\n");
	struct PMMFreeHeader* cur = g_PMM->LUT[0];
	while (cur)
	{
		DebugCon_WriteFormatted("  0x%016lX -> 0x%016lX(%lu)\n", (uint64_t) cur, (uint64_t) cur + cur->Count * 4096, cur->Count);
		cur = cur->Next;
	}

	DebugCon_WriteString("PMM LUT:\n");
	cur        = g_PMM->LUT[0];
	uint8_t pI = 0;
	for (uint8_t i = 1; i < 255; ++i)
	{
		struct PMMFreeHeader* next = g_PMM->LUT[i];
		if (next != cur)
		{
			if (cur)
				DebugCon_WriteFormatted("  %u -> %u: 0x%016lX -> 0x%016lX(%lu)\n", pI, i - 1, (uint64_t) cur, (uint64_t) cur + cur->Count * 4096, cur->Count);
			else
				DebugCon_WriteFormatted("  %u -> %u: nullptr\n", pI, i - 1);
			cur = next;
			pI  = i;
		}
	}
	if (cur)
		DebugCon_WriteFormatted("  %u -> 254: 0x%016lX -> 0x%016lX(%lu)\n", pI, (uint64_t) cur, (uint64_t) cur + cur->Count * 4096, cur->Count);
	else
		DebugCon_WriteFormatted("  %u -> 254: nullptr\n", pI);
}

void* PMMAlloc(size_t count)
{
	if (count == 0)
		return nullptr;

	struct PMMFreeHeader* header = PMMTakeFreeRange(count);
	if (!header)
		return nullptr;

	g_PMM->Stats.PagesFree -= count;
	uint64_t firstPage      = (uint64_t) header / 4096;
	if (count > 1)
		PMMBitmapSetRange(firstPage, firstPage + count - 1, false);
	else
		PMMBitmapSetEntry(firstPage, false);
	if (header->Count > count)
	{
		PMMFillFreePages(firstPage + count, firstPage + header->Count - 1);
		PMMInsertFreeRange((struct PMMFreeHeader*) ((firstPage + count) * 4096));
	}
	return (void*) (firstPage * 4096);
}

void* PMMAllocAligned(size_t count, uint8_t alignment)
{
	if (count == 0)
		return nullptr;
	if (alignment <= 12)
		return PMMAlloc(count);

	uint64_t alignmentVal  = 1UL << (alignment - 12);
	uint64_t alignmentMask = alignmentVal - 1;

	struct PMMFreeHeader* header = PMMTakeFreeRange(count + alignmentVal);
	if (!header)
	{
		header = PMMTakeAlignedRange(count, alignment);
		if (!header)
			return nullptr;
	}

	g_PMM->Stats.PagesFree -= count;
	uint64_t headerPage     = (uint64_t) header / 4096;
	uint64_t lastRangePage  = headerPage + header->Count - 1;
	uint64_t firstPage      = (headerPage + alignmentMask) & ~alignmentMask;
	uint64_t lastPage       = firstPage + count - 1;
	if (count > 1)
		PMMBitmapSetRange(firstPage, lastPage, false);
	else
		PMMBitmapSetEntry(firstPage, false);
	if (headerPage != firstPage)
	{
		PMMFillFreePages(headerPage, firstPage - 1);
		PMMInsertFreeRange(header);
	}
	if (lastPage != lastRangePage)
	{
		PMMFillFreePages(lastPage + 1, lastRangePage);
		PMMInsertFreeRange((struct PMMFreeHeader*) ((lastPage + 1) * 4096));
	}
	return (void*) (firstPage * 4096);
}

void PMMFree(void* address, size_t count)
{
	if (!address ||
		count == 0)
		return;

	uint64_t firstPage = (uint64_t) address / 4096;
	if (PMMBitmapGetEntry(firstPage))
		return;
	g_PMM->Stats.PagesFree += count;
	if (count > 1)
		PMMBitmapSetRange(firstPage, firstPage + count - 1, true);
	else
		PMMBitmapSetEntry(firstPage, true);

	uint64_t bottomPage = firstPage;
	uint64_t totalCount = count;
	if (PMMBitmapGetEntry(firstPage - 1))
	{
		struct PMMFreeHeader* header = PMMGetFirstPage((struct PMMFreeHeader*) ((firstPage - 1) * 4096));
		bottomPage                   = (uint64_t) header / 4096;
		totalCount                  += header->Count;
		PMMEraseFreeRange(header);
	}
	if (PMMBitmapGetEntry(firstPage + count))
	{
		struct PMMFreeHeader* header = (struct PMMFreeHeader*) ((firstPage + count) * 4096);
		totalCount                  += header->Count;
		PMMEraseFreeRange(header);
	}
	PMMFillFreePages(bottomPage, bottomPage + totalCount - 1);
	PMMInsertFreeRange((struct PMMFreeHeader*) (bottomPage * 4096));
}

#endif