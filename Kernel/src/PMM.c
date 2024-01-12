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
	size_t                    MemoryMapCount;
	struct PMMMemoryMapEntry* MemoryMap;

	size_t HighestAddress;

	uint64_t* Bitmap;

	struct PMMFreeHeader* Last;
	struct PMMFreeHeader* SkipList[255];
};

struct PMMState* g_PMM;

static uint8_t PMMGetSkipListIndex(uint64_t count)
{
	if (count < 193)
		return (uint8_t) count - 1;
	return (64 - __builtin_clz(count - 192)) + 190;
}

static uint8_t PMMGetSkipListCeilIndex(uint64_t count)
{
	if (count < 193)
		return (uint8_t) count - 1;
	uint64_t x = count - 192;
	return (64 - __builtin_clz(x)) + 190 + (x & (x - 1) ? 1 : 0);
}

static uint64_t PMMGetSkipListValue(uint8_t index)
{
	if (index < 192)
		return index + 1;
	return (1UL << (index - 191)) + 192;
}

static struct PMMFreeHeader* PMMFindFreeRange(uint64_t count)
{
	uint8_t index = PMMGetSkipListCeilIndex(count);
	if (g_PMM->SkipList[index])
		return g_PMM->SkipList[index];
	if (index == 0)
		return nullptr;
	uint64_t minValue = PMMGetSkipListValue(index);
	if (count >= minValue)
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
		DebugCon_WriteFormatted("Index %hhu Header 0x%016X, Other 0x%016X, Prev 0x%016X\r\n", index, (uint64_t) header, (uint64_t) other, (uint64_t) other->Prev);
		if (other->Prev)
			other->Prev->Next = header;
		header->Prev           = other->Prev;
		header->Next           = other;
		other->Prev            = header;
		g_PMM->SkipList[index] = header;
		return;
	}

	for (uint16_t i = index + 1; i-- > 0;)
	{
		if (g_PMM->SkipList[i])
			break;
		g_PMM->SkipList[i] = header;
	}
	header->Prev = g_PMM->Last;
	header->Next = nullptr;
	if (g_PMM->Last)
		g_PMM->Last->Next = header;
	g_PMM->Last = header;
	DebugCon_WriteFormatted("Index %hhu Header 0x%016X\r\n", index, (uint64_t) header);
}

static void PMMEraseFreeRange(struct PMMFreeHeader* header)
{
	if (header == g_PMM->Last)
		g_PMM->Last = header->Prev;
	uint8_t index = PMMGetSkipListIndex(header->Count);
	if (g_PMM->SkipList[index] == header)
	{
		for (uint16_t i = index + 1; i-- > 0;)
		{
			if (g_PMM->SkipList[i] != header)
				break;
			g_PMM->SkipList[i] = header->Next;
		}
	}

	if (header->Prev)
		header->Prev->Next = header->Next;
	if (header->Next)
		header->Next->Prev = header->Prev;
	header->Next = nullptr;
	header->Prev = nullptr;
}

void PMMInit(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata)
{
	uint64_t highestAddress = 0;
	for (size_t i = 0; i < entryCount; ++i)
	{
		struct PMMMemoryMapEntry entry;
		if (!getter(userdata, i, &entry))
			continue;
		if (entry.Start + entry.Size > highestAddress)
			highestAddress = entry.Start + entry.Size;
	}
	size_t pmmRequiredSize = (sizeof(struct PMMState) + sizeof(struct PMMMemoryMapEntry) * entryCount + (highestAddress / 32768) + 4095) & ~0xFFF; // (32768 = 4 KiB page size * 8 bits)
	size_t pmmAllocatedIn  = ~0UL;
	for (size_t i = 0; i < entryCount; ++i)
	{
		struct PMMMemoryMapEntry entry;
		if (!getter(userdata, i, &entry))
			continue;
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
	g_PMM->MemoryMapCount = entryCount;
	g_PMM->MemoryMap      = (struct PMMMemoryMapEntry*) ((uint8_t*) g_PMM + sizeof(struct PMMState));
	g_PMM->HighestAddress = highestAddress;
	g_PMM->Bitmap         = (uint64_t*) ((uint8_t*) g_PMM + sizeof(struct PMMState) + sizeof(struct PMMMemoryMapEntry) * entryCount);
	memset(g_PMM->Bitmap, 0xFF, highestAddress / 32768);
	memset(g_PMM->SkipList, 0, sizeof(g_PMM->SkipList));
	for (size_t i = 0; i < entryCount; ++i)
	{
		struct PMMMemoryMapEntry* entry = g_PMM->MemoryMap + i;
		if (!getter(userdata, i, entry))
		{
			entry->Start = ~0UL;
			entry->Size  = 0;
			entry->Type  = PMMMemoryMapTypeInvalid;
			continue;
		}
		if (pmmAllocatedIn == i)
		{
			entry->Start += pmmRequiredSize;
			entry->Size  -= pmmRequiredSize;
		}
		// if (!(entry->Type & PMMMemoryMapTypeFree))
		if (entry->Type != PMMMemoryMapTypeFree)
			continue;

		size_t firstBit = entry->Start / 4096;
		size_t lastBit  = (entry->Start + entry->Size) / 4096;

		size_t   firstEntry = firstBit / 64;
		size_t   lastEntry  = lastBit / 64;
		uint64_t firstMask  = ~0UL << (firstBit & 63);
		uint64_t lastMask   = ~0U >> (64 - lastBit & 63);
		if (firstEntry == lastEntry)
		{
			uint64_t mask              = firstMask & lastMask;
			g_PMM->Bitmap[firstEntry] &= ~mask;
		}
		else
		{
			g_PMM->Bitmap[firstEntry] &= ~firstMask;
			memset(g_PMM->Bitmap + firstEntry + 1, 0x00, (lastEntry - firstEntry - 1));
			g_PMM->Bitmap[lastEntry] &= ~lastMask;
		}

		struct PMMFreeHeader* firstFreeHeader = (struct PMMFreeHeader*) entry->Start;
		firstFreeHeader->Count                = entry->Size / 4096;
		if (firstFreeHeader->Count > 1)
		{
			struct PMMFreeHeader* lastFreeHeader = (struct PMMFreeHeader*) (entry->Start + (firstFreeHeader->Count - 1) * 4096);
			lastFreeHeader->Count                = -firstFreeHeader->Count;
		}
		PMMInsertFreeRange(firstFreeHeader);
	}
}

void PMMPrintFreeList()
{
	uint8_t               i         = 0;
	struct PMMFreeHeader* firstPage = g_PMM->SkipList[0];
	while (firstPage)
	{
		DebugCon_WriteFormatted("0x%016X -> 0x%016X(%lu), ", (uint64_t) firstPage, (uint64_t) firstPage + firstPage->Count * 4096, firstPage->Count);
		firstPage = firstPage->Next;
	}
	DebugCon_WriteChars("\r\n", 2);
}

void* PMMAlloc()
{
	struct PMMFreeHeader* page = g_PMM->SkipList[0];
	if (!page)
		return nullptr;
	PMMEraseFreeRange(page);

	if (page->Count > 1)
	{
		struct PMMFreeHeader* firstFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) page + 4096);
		firstFreeHeader->Count                = page->Count - 1;
		if (firstFreeHeader->Count > 1)
		{
			struct PMMFreeHeader* lastFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) page + (page->Count - 1) * 4096);
			lastFreeHeader->Count                = -firstFreeHeader->Count;
		}
		PMMInsertFreeRange(firstFreeHeader);
	}

	uint64_t bitmapBit             = (uint64_t) page / 4096;
	g_PMM->Bitmap[bitmapBit >> 6] |= 1UL << (bitmapBit & 63);
	return (void*) page;
}

void PMMFree(void* address)
{
	address            = (void*) ((uint64_t) address & ~0xFFF);
	uint64_t bitmapBit = (uint64_t) address / 4096;
	if ((g_PMM->Bitmap[bitmapBit >> 6] >> (bitmapBit & 63)) & 1)
		return;

	g_PMM->Bitmap[bitmapBit >> 6] &= ~(1UL << (bitmapBit & 63));

	uint64_t belowBitmapBit = bitmapBit - 1;
	uint64_t aboveBitmapBit = bitmapBit + 1;
	bool     hasBelow       = (g_PMM->Bitmap[belowBitmapBit >> 6] >> (belowBitmapBit & 63)) & 1;
	bool     hasAbove       = (g_PMM->Bitmap[aboveBitmapBit >> 6] >> (aboveBitmapBit & 63)) & 1;

	struct PMMFreeHeader* bottomFreeHeader = (struct PMMFreeHeader*) address;
	uint64_t              totalCount       = 1;
	if (hasBelow)
	{
		struct PMMFreeHeader* belowFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) address - 4096);
		if (belowFreeHeader->Count < 0)
		{
			bottomFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) belowFreeHeader - belowFreeHeader->Count * 4096);
			totalCount      -= belowFreeHeader->Count;
		}
		else
		{
			bottomFreeHeader = belowFreeHeader;
			totalCount      += belowFreeHeader->Count;
		}
		PMMEraseFreeRange(belowFreeHeader);
	}
	if (hasAbove)
	{
		struct PMMFreeHeader* aboveFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) address + 4096);
		totalCount                           += aboveFreeHeader->Count;
		PMMEraseFreeRange(aboveFreeHeader);
	}

	bottomFreeHeader->Count = totalCount;
	if (bottomFreeHeader->Count > 1)
	{
		struct PMMFreeHeader* lastFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) bottomFreeHeader + (bottomFreeHeader->Count - 1) * 4096);
		lastFreeHeader->Count                = -bottomFreeHeader->Count;
	}
	PMMInsertFreeRange(bottomFreeHeader);
}

void* PMMAllocContiguous(size_t count)
{
	if (count == 0)
		return nullptr;
	struct PMMFreeHeader* page = PMMFindFreeRange(count);
	if (!page)
		return nullptr;
	PMMEraseFreeRange(page);

	if (page->Count > count)
	{
		struct PMMFreeHeader* firstFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) page + (count * 4096));
		firstFreeHeader->Count                = page->Count - count;
		if (firstFreeHeader->Count > 1)
		{
			struct PMMFreeHeader* lastFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) page + (page->Count - 1) * 4096);
			lastFreeHeader->Count                = -firstFreeHeader->Count;
		}
		PMMInsertFreeRange(firstFreeHeader);
	}

	uint64_t firstBit = (uint64_t) page / 4096;
	uint64_t lastBit  = firstBit + count;

	uint64_t firstEntry = firstBit / 64;
	uint64_t lastEntry  = lastBit / 64;
	uint64_t firstMask  = ~0UL << (firstBit & 63);
	uint64_t lastMask   = ~0U >> (64 - lastBit & 63);
	if (firstEntry == lastEntry)
	{
		uint64_t mask              = firstMask & lastMask;
		g_PMM->Bitmap[firstEntry] |= mask;
	}
	else
	{
		g_PMM->Bitmap[firstEntry] |= firstMask;
		memset(g_PMM->Bitmap + firstEntry + 1, 0xFF, (lastEntry - firstEntry - 1));
		g_PMM->Bitmap[lastEntry] |= lastMask;
	}
	return (void*) page;
}

void PMMFreeContiguous(void* address, size_t count)
{
	if (count == 0)
		return;
	address            = (void*) ((uint64_t) address & ~0xFFF);
	uint64_t bitmapBit = (uint64_t) address / 4096;
	if ((g_PMM->Bitmap[bitmapBit >> 6] >> (bitmapBit & 63)) & 1)
		return;

	size_t lastBit = bitmapBit + count;

	size_t   firstEntry = bitmapBit / 64;
	size_t   lastEntry  = lastBit / 64;
	uint64_t firstMask  = ~0UL << (bitmapBit & 63);
	uint64_t lastMask   = ~0U >> (64 - lastBit & 63);
	if (firstEntry == lastEntry)
	{
		uint64_t mask              = firstMask & lastMask;
		g_PMM->Bitmap[firstEntry] &= ~mask;
	}
	else
	{
		g_PMM->Bitmap[firstEntry] &= ~firstMask;
		memset(g_PMM->Bitmap + firstEntry + 1, 0x00, (lastEntry - firstEntry - 1));
		g_PMM->Bitmap[lastEntry] &= ~lastMask;
	}

	uint64_t belowBitmapBit = bitmapBit - 1;
	uint64_t aboveBitmapBit = lastBit + 1;
	bool     hasBelow       = (g_PMM->Bitmap[belowBitmapBit >> 6] >> (belowBitmapBit & 63)) & 1;
	bool     hasAbove       = (g_PMM->Bitmap[aboveBitmapBit >> 6] >> (aboveBitmapBit & 63)) & 1;

	struct PMMFreeHeader* bottomFreeHeader = (struct PMMFreeHeader*) address;
	uint64_t              totalCount       = count;
	if (hasBelow)
	{
		struct PMMFreeHeader* belowFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) address - 4096);
		if (belowFreeHeader->Count < 0)
		{
			bottomFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) belowFreeHeader - belowFreeHeader->Count * 4096);
			totalCount      -= belowFreeHeader->Count;
		}
		else
		{
			bottomFreeHeader = belowFreeHeader;
			totalCount      += belowFreeHeader->Count;
		}
		PMMEraseFreeRange(belowFreeHeader);
	}
	if (hasAbove)
	{
		struct PMMFreeHeader* aboveFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) address + count * 4096);
		totalCount                           += aboveFreeHeader->Count;
		PMMEraseFreeRange(aboveFreeHeader);
	}

	bottomFreeHeader->Count = totalCount;
	if (bottomFreeHeader->Count > 1)
	{
		struct PMMFreeHeader* lastFreeHeader = (struct PMMFreeHeader*) ((uint8_t*) bottomFreeHeader + (bottomFreeHeader->Count - 1) * 4096);
		lastFreeHeader->Count                = -bottomFreeHeader->Count;
	}
	PMMInsertFreeRange(bottomFreeHeader);
}