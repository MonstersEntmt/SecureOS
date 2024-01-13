#include "PMM.h"
#include "DebugCon.h"

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
	struct PMMFreeHeader* SkipList[255];
};

struct PMMState* g_PMM;

static uint64_t PMMGetSkipListValue(uint8_t index)
{
	if (index < 192)
		return (uint64_t) index + 1;
	return (1UL << (index - 191)) + 192;
}

static uint8_t PMMGetSkipListIndex(uint64_t value)
{
	if (value < 193)
		return (uint8_t) value - 1;
	return (254 - __builtin_clz(value - 192));
}

static uint8_t PMMGetSkipListCeilIndex(uint64_t value)
{
	if (value < 193)
		return (uint8_t) value - 1;
	return (255 - __builtin_clz(value - 193));
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
	struct PMMFreeHeader* lastHeader  = (struct PMMFreeHeader*) (lastPage * 4096);

	uint64_t pageCount = lastPage - firstPage + 1;
	firstHeader->Count = pageCount;
	firstHeader->Prev  = nullptr;
	firstHeader->Next  = nullptr;
	lastHeader->Count  = -pageCount;
	lastHeader->Prev   = nullptr;
	lastHeader->Next   = nullptr;
}

static struct PMMFreeHeader* PMMGetFirstPage(struct PMMFreeHeader* header)
{
	if (header->Count < 0)
		return (struct PMMFreeHeader*) ((uint8_t*) header + (header->Count + 1) * 4096);
	return header;
}

static struct PMMFreeHeader* PMMFindFreeRange(uint64_t count)
{
	uint8_t index = PMMGetSkipListCeilIndex(count);
	if (g_PMM->SkipList[index])
		return g_PMM->SkipList[index];

	if (index == 0)
		return nullptr;

	uint64_t value = PMMGetSkipListValue(index);
	if (value == count)
		return nullptr;

	struct PMMFreeHeader* cur = g_PMM->SkipList[index - 1];
	while (cur && cur->Count < count)
		cur = cur->Next;
	return cur;
}

static void PMMInsertFreeRange(struct PMMFreeHeader* header)
{
	uint8_t index = PMMGetSkipListIndex(header->Count);
	if (g_PMM->SkipList[index])
	{
		struct PMMFreeHeader* other = g_PMM->SkipList[index];
		if (other->Prev)
			other->Prev->Next = header;
		header->Next = other;
		header->Prev = other->Prev;
		// DebugCon_WriteFormatted("Inserting 0x%016lX such that 0x%016lX -> 0x%016lX -> 0x%016lX", (uint64_t) header, (uint64_t) other->Prev, (uint64_t) header, (uint64_t) other);
		other->Prev = header;
		uint8_t i   = index + 1;
		for (; i-- > 0;)
		{
			if (g_PMM->SkipList[i] != other)
				break;
			g_PMM->SkipList[i] = header;
		}
		// DebugCon_WriteFormatted(" Filled SkipList from %u to %u\n", index, (uint8_t) (i + 1));
		return;
	}

	uint8_t i = index + 1;
	for (; i-- > 0;)
	{
		if (g_PMM->SkipList[i])
			break;
		g_PMM->SkipList[i] = header;
	}
	if (g_PMM->Last)
		g_PMM->Last->Next = header;
	// DebugCon_WriteFormatted("Appending 0x%016lX such that 0x%016lX -> 0x%016lX -> nullptr Filled SkipList from %u to %u\n", (uint64_t) header, (uint64_t) g_PMM->Last, (uint64_t) header, index, (uint8_t) (i + 1));
	header->Prev = g_PMM->Last;
	header->Next = nullptr;
	g_PMM->Last  = header;
}

static void PMMEraseFreeRange(struct PMMFreeHeader* header)
{
	if (g_PMM->Last == header)
		g_PMM->Last = header->Prev;
	uint8_t index = PMMGetSkipListIndex(header->Count);
	for (uint8_t i = index + 1; i-- > 0;)
	{
		if (g_PMM->SkipList[i] != header)
			break;
		g_PMM->SkipList[i] = header->Next;
	}

	if (header->Prev)
		header->Prev->Next = header->Next;
	if (header->Next)
		header->Next->Prev = header->Prev;
	header->Prev = nullptr;
	header->Next = nullptr;
}

void PMMInit(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata)
{
	uint64_t lastUsableAddress = 0;
	uint64_t lastAddress       = 0;
	for (size_t i = 0; i < entryCount; ++i)
	{
		struct PMMMemoryMapEntry entry;
		if (!getter(userdata, i, &entry))
			continue;

		if (entry.Start + entry.Size > lastAddress)
			lastAddress = entry.Start + entry.Size;

		if (!(entry.Type & PMMMemoryMapTypeUsable))
			continue;

		if (entry.Start + entry.Size > lastUsableAddress)
			lastUsableAddress = entry.Start + entry.Size;
	}

	size_t pmmRequiredSize = sizeof(struct PMMState) + ((lastUsableAddress + 32767) / 32768); // (32768 = 4 KiB page size * 8 bits for bitmap)
	pmmRequiredSize        = (pmmRequiredSize + 4095) & ~0xFFFUL;
	size_t pmmAllocatedIn  = ~0UL;
	for (size_t i = 0; i < entryCount; ++i)
	{
		struct PMMMemoryMapEntry entry;
		if (!getter(userdata, i, &entry))
			continue;

		if (entry.Start == 0)
		{
			entry.Start += 4096;
			entry.Size  -= 4096;
		}
		if (entry.Size <= pmmRequiredSize)
			continue;
		pmmAllocatedIn = i;
		g_PMM          = (struct PMMState*) entry.Start;
		break;
	}
	if (pmmAllocatedIn >= entryCount)
	{
		// TODO(MarcasRealAccount): Panic!
		return;
	}

	g_PMM->Stats = (struct PMMMemoryStats) {
		.LastUsableAddress = lastUsableAddress,
		.LastAddress       = lastAddress,
		.PagesFree         = 0
	};
	g_PMM->MemoryMapCount = 0;
	g_PMM->MemoryMap      = nullptr;
	g_PMM->Bitmap         = (uint64_t*) ((uint8_t*) g_PMM + sizeof(struct PMMState));
	memset(g_PMM->Bitmap, 0, lastUsableAddress / 32768);
	memset(g_PMM->SkipList, 0, sizeof(g_PMM->SkipList));

	for (size_t i = 0; i < entryCount; ++i)
	{
		struct PMMMemoryMapEntry entry;
		if (!getter(userdata, i, &entry))
			continue;

		if (entry.Type != PMMMemoryMapTypeUsable)
			continue;

		if (entry.Start == 0)
		{
			entry.Start += 4096;
			entry.Size  -= 4096;
		}
		if (pmmAllocatedIn == i)
		{
			entry.Start += pmmRequiredSize;
			entry.Size  -= pmmRequiredSize;
		}

		PMMFreeContiguous((void*) entry.Start, entry.Size / 4096);
	}

	g_PMM->MemoryMapCount    = entryCount + 2;
	g_PMM->MemoryMap         = PMMAllocContiguous((g_PMM->MemoryMapCount + 4095) / 4096);
	size_t curMemoryMapEntry = 0;
	for (size_t i = 0; i < entryCount; ++i)
	{
		struct PMMMemoryMapEntry entry;
		if (!getter(userdata, i, &entry))
			continue;

		if (entry.Start == 0)
		{
			g_PMM->MemoryMap[curMemoryMapEntry++] = (struct PMMMemoryMapEntry) {
				.Start = 0,
				.Size  = 4096,
				.Type  = PMMMemoryMapTypeNullGuard
			};
			entry.Start += 4096;
			entry.Size  -= 4096;
		}
		if (pmmAllocatedIn == i)
		{
			g_PMM->MemoryMap[curMemoryMapEntry++] = (struct PMMMemoryMapEntry) {
				.Start = entry.Start,
				.Size  = pmmRequiredSize,
				.Type  = PMMMemoryMapTypePMM
			};
			entry.Start += pmmRequiredSize;
			entry.Size  -= pmmRequiredSize;
		}
		if (entry.Size > 0)
		{
			if (entry.Type == PMMMemoryMapTypeUsable)
				entry.Type = PMMMemoryMapTypeTaken;
			g_PMM->MemoryMap[curMemoryMapEntry++] = (struct PMMMemoryMapEntry) {
				.Start = entry.Start,
				.Size  = entry.Size,
				.Type  = entry.Type
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

		PMMFreeContiguous((void*) entry->Start, entry->Size / 4096);
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
	*stats = g_PMM->Stats;
}

size_t PMMGetMemoryMap(struct PMMMemoryMapEntry** entries)
{
	if (!entries)
		return 0;
	*entries = g_PMM->MemoryMap;
	return g_PMM->MemoryMapCount;
}

void PMMPrintMemoryMap()
{
	DebugCon_WriteFormatted("Memory Map:\n");
	for (size_t i = 0; i < g_PMM->MemoryMapCount; ++i)
	{
		struct PMMMemoryMapEntry* entry = &g_PMM->MemoryMap[i];
		const char*               entryType;
		switch (entry->Type)
		{
		case PMMMemoryMapTypeInvalid: entryType = "Invalid"; break;
		case PMMMemoryMapTypeUsable: entryType = "Usable"; break;
		case PMMMemoryMapTypeReclaimable: entryType = "Reclaimable"; break;
		case PMMMemoryMapTypeLoaderReclaimable: entryType = "Loader Reclaimable"; break;
		case PMMMemoryMapTypeTaken: entryType = "Taken"; break;
		case PMMMemoryMapTypeNullGuard: entryType = "Null Guard"; break;
		case PMMMemoryMapTypePMM: entryType = "PMM"; break;
		case PMMMemoryMapTypeKernel: entryType = "Kernel"; break;
		case PMMMemoryMapTypeModule: entryType = "Module"; break;
		case PMMMemoryMapTypeReserved: entryType = "Reserved"; break;
		case PMMMemoryMapTypeNVS: entryType = "NVS"; break;
		default: entryType = "INVALID!!!"; break;
		}

		DebugCon_WriteFormatted("%20s: 0x%016lX -> 0x%016lX(%lu)\n", entryType, entry->Start, entry->Start + entry->Size, entry->Size / 4096);
	}
}

void PMMPrintFreeList()
{
	uint8_t               i         = 0;
	struct PMMFreeHeader* firstPage = g_PMM->SkipList[0];
	while (firstPage)
	{
		DebugCon_WriteFormatted("0x%016lX -> 0x%016lX(%lu), ", (uint64_t) firstPage, (uint64_t) firstPage + firstPage->Count * 4096, firstPage->Count);
		firstPage = firstPage->Next;
	}
	DebugCon_WriteChars("\r\n", 2);
}

void* PMMAlloc()
{
	struct PMMFreeHeader* header = g_PMM->SkipList[0];
	if (!header)
		return nullptr;
	PMMEraseFreeRange(header);
	uint64_t page = (uint64_t) header / 4096;
	PMMBitmapSetEntry(page, false);
	--g_PMM->Stats.PagesFree;

	if (header->Count > 1)
	{
		PMMFillFreePages(page + 1, page + header->Count - 1);
		PMMInsertFreeRange((struct PMMFreeHeader*) ((page + 1) * 4096));
	}
	return (void*) header;
}

void PMMFree(void* address)
{
	uint64_t page = (uint64_t) address / 4096;
	if (PMMBitmapGetEntry(page))
		return;
	PMMBitmapSetEntry(page, true);
	++g_PMM->Stats.PagesFree;

	uint64_t bottomPage = page;
	uint64_t totalCount = 1;
	if (PMMBitmapGetEntry(page - 1))
	{
		struct PMMFreeHeader* header = PMMGetFirstPage((struct PMMFreeHeader*) ((page - 1) * 4096));
		bottomPage                   = (uint64_t) header / 4096;
		totalCount                  += header->Count;
		PMMEraseFreeRange(header);
	}
	if (PMMBitmapGetEntry(page + 1))
	{
		struct PMMFreeHeader* header = (struct PMMFreeHeader*) ((page + 1) * 4096);
		totalCount                  += header->Count;
		PMMEraseFreeRange(header);
	}
	PMMFillFreePages(bottomPage, bottomPage + totalCount - 1);
	PMMInsertFreeRange((struct PMMFreeHeader*) (bottomPage * 4096));
}

void* PMMAllocContiguous(size_t count)
{
	if (count == 0)
		return nullptr;
	if (count == 1)
		return PMMAlloc();

	struct PMMFreeHeader* header = PMMFindFreeRange(count);
	if (!header)
		return nullptr;
	PMMEraseFreeRange(header);
	uint64_t firstPage = (uint64_t) header / 4096;
	uint64_t lastPage  = firstPage + count - 1;
	PMMBitmapSetRange(firstPage, lastPage, false);
	g_PMM->Stats.PagesFree -= count;

	if (header->Count > count)
	{
		PMMFillFreePages(firstPage + count, firstPage + header->Count - 1);
		PMMInsertFreeRange((struct PMMFreeHeader*) ((firstPage + count) * 4096));
	}
	return (void*) header;
}

void PMMFreeContiguous(void* address, size_t count)
{
	if (count == 0)
		return;
	if (count == 1)
		return PMMFree(address);

	uint64_t firstPage = (uint64_t) address / 4096;
	if (PMMBitmapGetEntry(firstPage))
		return;
	PMMBitmapSetRange(firstPage, firstPage + count - 1, true);
	g_PMM->Stats.PagesFree += count;

	// DebugCon_WriteFormatted("Freeing   0x%016lX(%lu)", (uint64_t) address, count);
	uint64_t bottomPage = firstPage;
	uint64_t totalCount = count;
	if (PMMBitmapGetEntry(firstPage - 1))
	{
		struct PMMFreeHeader* header = PMMGetFirstPage((struct PMMFreeHeader*) ((firstPage - 1) * 4096));
		bottomPage                   = (uint64_t) header / 4096;
		totalCount                  += header->Count;
		// DebugCon_WriteFormatted(" Coalesce down %lu pages with 0x%016lX", header->Count, (uint64_t) header);
		PMMEraseFreeRange(header);
	}
	if (PMMBitmapGetEntry(firstPage + count))
	{
		struct PMMFreeHeader* header = (struct PMMFreeHeader*) ((firstPage + count) * 4096);
		totalCount                  += header->Count;
		// DebugCon_WriteFormatted(" Coalesce up %lu pages", header->Count);
		PMMEraseFreeRange(header);
	}
	// DebugCon_WriteFormatted(" Forming 0x%016lX -> 0x%016lX(%lu)\n", bottomPage * 4096, (bottomPage + totalCount) * 4096, totalCount);
	PMMFillFreePages(bottomPage, bottomPage + totalCount - 1);
	PMMInsertFreeRange((struct PMMFreeHeader*) (bottomPage * 4096));
}