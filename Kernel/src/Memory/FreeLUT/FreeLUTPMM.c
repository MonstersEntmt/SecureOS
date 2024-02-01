#include "Memory/FreeLUT/FreeLUTPMM.h"
#include "Memory/FreeLUT/FreeLUT.h"
#include "Memory/PMM.h"
#include "Panic.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct FreeLUTPageHeader
{
	ssize_t                   Count;
	struct FreeLUTPageHeader* Prev;
	struct FreeLUTPageHeader* Next;
};

struct FreeLUTState
{
	struct PMMMemoryStats Stats;

	size_t                    MemoryMapCount;
	struct PMMMemoryMapEntry* MemoryMap;

	uint64_t*                 Bitmap;
	struct FreeLUTPageHeader* Last;
	struct FreeLUTPageHeader* LUT[255];
};

struct FreeLUTState* g_FreeLUT;

static void FreeLUTLock(void)
{
}

static void FreeLUTUnlock(void)
{
}

static bool FreeLUTBitmapGetEntry(uintptr_t page)
{
	return (g_FreeLUT->Bitmap[page >> 6] >> (page & 63)) & 1;
}

static void FreeLUTBitmapSetEntry(uintptr_t page, bool value)
{
	const uintptr_t index = page >> 6;
	const uint64_t  temp2 = 1UL << (page & 63);
	const uint64_t  temp3 = ~temp2;
	uint64_t*       temp  = &g_FreeLUT->Bitmap[index];
	*temp                 = value ? (*temp | temp2) : (*temp & temp2);
}

static void FreeLUTBitmapSetRange(uintptr_t firstPage, uintptr_t lastPage, bool value)
{
	const uint64_t firstIndex = firstPage >> 6;
	const uint64_t lastIndex  = lastPage >> 6;

	const uint64_t lowMask  = ~0UL << (firstPage & 63);
	const uint64_t highMask = ~0UL >> (63 - (lastPage & 63));
	if (firstIndex == lastIndex)
	{
		uint64_t*      temp  = &g_FreeLUT->Bitmap[firstIndex];
		const uint64_t temp2 = lowMask & highMask;
		const uint64_t temp3 = ~temp2;
		*temp                = value ? (*temp | temp2) : (*temp & temp3);
	}
	else
	{
		if (value)
		{
			g_FreeLUT->Bitmap[firstIndex] |= lowMask;
			g_FreeLUT->Bitmap[lastIndex]  |= highMask;
		}
		else
		{
			g_FreeLUT->Bitmap[firstIndex] &= ~lowMask;
			g_FreeLUT->Bitmap[lastIndex]  &= ~highMask;
		}
		if (firstIndex + 1 < lastIndex)
			memset(&g_FreeLUT->Bitmap[firstIndex + 1], value ? 0xFF : 0x00, (lastIndex - firstIndex - 1) * 8);
	}
}

static size_t FreeLUTBitmapCount(uintptr_t firstPage, uintptr_t lastPage, bool value)
{
	// TODO(MarcasRealAccount): Implement bit counting
	// Potentially by counting how many bits are set within the range, then based on value either return it as is or as max - count to flip to how many bits are not set
	return lastPage - firstPage;
}

static void FreeLUTFillFreePages(uintptr_t firstPage, uintptr_t lastPage)
{
	struct FreeLUTPageHeader* firstHeader = (struct FreeLUTPageHeader*) (firstPage << 12);

	const size_t pageCount = lastPage - firstPage + 1;
	firstHeader->Count     = pageCount;
	firstHeader->Prev      = nullptr;
	firstHeader->Next      = nullptr;
	if (firstPage != lastPage)
	{
		struct FreeLUTPageHeader* lastHeader = (struct FreeLUTPageHeader*) (lastPage << 12);
		lastHeader->Count                    = -pageCount;
		lastHeader->Prev                     = nullptr;
		lastHeader->Next                     = nullptr;
	}
}

static struct FreeLUTPageHeader* FreeLUTGetFirstPage(struct FreeLUTPageHeader* header)
{
	if (header->Count >= 0)
		return header;
	return (struct FreeLUTPageHeader*) ((uintptr_t) header + ((header->Count + 1) << 12));
}

static void FreeLUTInsertFreePage(struct FreeLUTPageHeader* header)
{
	uint8_t index = FreeLUTGetFloorIndex(header->Count);
	FreeLUTLock();
	if (g_FreeLUT->LUT[index])
	{
		struct FreeLUTPageHeader* other = g_FreeLUT->LUT[index];
		if (other->Prev)
			other->Prev->Next = header;
		header->Next = other;
		header->Prev = other->Prev;
		other->Prev  = header;
		for (uint8_t i = index + 1; i-- > 0;)
		{
			if (g_FreeLUT->LUT[i] != other)
				break;
			g_FreeLUT->LUT[i] = header;
		}
		FreeLUTUnlock();
		return;
	}

	for (uint8_t i = index + 1; i-- > 0;)
	{
		if (g_FreeLUT->LUT[i])
			break;
		g_FreeLUT->LUT[i] = header;
	}
	if (g_FreeLUT->Last)
		g_FreeLUT->Last->Next = header;
	header->Prev    = g_FreeLUT->Last;
	header->Next    = nullptr;
	g_FreeLUT->Last = header;
	FreeLUTUnlock();
}

static void FreeLUTEraseFreeRange(struct FreeLUTPageHeader* header)
{
	uint8_t index = FreeLUTGetFloorIndex(header->Count);
	FreeLUTLock();
	if (g_FreeLUT->Last == header)
		g_FreeLUT->Last = header->Prev;
	for (uint8_t i = index + 1; i-- > 0;)
	{
		if (g_FreeLUT->LUT[i] != header)
			break;
		g_FreeLUT->LUT[i] = header->Next;
	}

	if (header->Prev)
		header->Prev->Next = header->Next;
	if (header->Next)
		header->Next->Prev = header->Prev;
	header->Prev = nullptr;
	header->Next = nullptr;
	FreeLUTUnlock();
}

static struct FreeLUTPageHeader* FreeLUTTakeFreeRange(size_t count)
{
	FreeLUTLock();
	if (count == 1)
	{
		struct FreeLUTPageHeader* header = g_FreeLUT->LUT[0];
		if (header)
			FreeLUTEraseFreeRange(header);
		FreeLUTUnlock();
		return header;
	}

	uint8_t index = FreeLUTGetCeilIndex(count);
	if (g_FreeLUT->LUT[index])
	{
		struct FreeLUTPageHeader* header = g_FreeLUT->LUT[index];
		FreeLUTEraseFreeRange(header);
		FreeLUTUnlock();
		return header;
	}

	if (index == 0 ||
		FreeLUTGetValue(index) == count)
	{
		FreeLUTUnlock();
		return nullptr;
	}

	struct FreeLUTPageHeader* cur = g_FreeLUT->LUT[index - 1];
	while (cur && cur != g_FreeLUT->LUT[index] && cur->Count < count)
		cur = cur->Next;
	if (!cur && cur == g_FreeLUT->LUT[index])
	{
		FreeLUTUnlock();
		return nullptr;
	}
	FreeLUTEraseFreeRange(cur);
	FreeLUTUnlock();
	return cur;
}

static struct FreeLUTPageHeader* FreeLUTTakeFreeRangeBelow(size_t count, void* largestAddress)
{
	largestAddress = (void*) ((uintptr_t) largestAddress - (count << 12));
	FreeLUTLock();
	if (count == 1)
	{
		struct FreeLUTPageHeader* header = g_FreeLUT->LUT[0];
		while (header && (uintptr_t) header >= (uintptr_t) largestAddress)
			header = header->Next;
		if (header)
			FreeLUTEraseFreeRange(header);
		FreeLUTUnlock();
		return header;
	}

	uint8_t index = FreeLUTGetCeilIndex(count);
	if (g_FreeLUT->LUT[index])
	{
		struct FreeLUTPageHeader* header = g_FreeLUT->LUT[index];
		while (header && (uintptr_t) header >= (uintptr_t) largestAddress)
			header = header->Next;
		FreeLUTEraseFreeRange(header);
		FreeLUTUnlock();
		return header;
	}

	if (index == 0 ||
		FreeLUTGetValue(index) == count)
	{
		FreeLUTUnlock();
		return nullptr;
	}

	struct FreeLUTPageHeader* cur = g_FreeLUT->LUT[index - 1];
	while (cur && cur != g_FreeLUT->LUT[index] && cur->Count < count && (uintptr_t) cur >= (uintptr_t) largestAddress)
		cur = cur->Next;
	if (!cur && cur == g_FreeLUT->LUT[index])
	{
		FreeLUTUnlock();
		return nullptr;
	}
	FreeLUTEraseFreeRange(cur);
	FreeLUTUnlock();
	return cur;
}

static struct FreeLUTPageHeader* FreeLUTTakeFreeRangeAligned(uintptr_t count, uint8_t alignment)
{
	uint8_t index = FreeLUTGetFloorIndex(count);
	FreeLUTLock();
	struct FreeLUTPageHeader* cur = g_FreeLUT->LUT[index];

	const uintptr_t alignmentVal  = 1UL << alignment;
	const uintptr_t alignmentMask = alignmentVal - 1;
	const uintptr_t temp          = ~alignmentMask;
	while (cur && (((uintptr_t) cur + alignmentMask) & temp) > ((uintptr_t) cur + (cur->Count << 12)))
		cur = cur->Next;
	if (cur)
		FreeLUTEraseFreeRange(cur);
	FreeLUTUnlock();
	return cur;
}

static struct FreeLUTPageHeader* FreeLUTTakeFreeRangeAlignedBelow(uintptr_t count, uint8_t alignment, void* largestAddress)
{
	largestAddress = (void*) ((uintptr_t) largestAddress - (count << 12));
	FreeLUTLock();
	uint8_t                   index = FreeLUTGetFloorIndex(count);
	struct FreeLUTPageHeader* cur   = g_FreeLUT->LUT[index];

	const uintptr_t alignmentVal  = 1UL << alignment;
	const uintptr_t alignmentMask = alignmentVal - 1;
	const uintptr_t temp          = ~alignmentMask;
	while (cur && (((uintptr_t) cur + alignmentMask) & temp) > ((uintptr_t) cur + (cur->Count << 12)) && (uintptr_t) cur > (uintptr_t) largestAddress)
		cur = cur->Next;
	if (cur)
		FreeLUTEraseFreeRange(cur);
	FreeLUTUnlock();
	return cur;
}

void FreeLUTPMMInit(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata)
{
	struct PMMMemoryMapEntry tempMemoryMapEntry;

	getter(userdata, entryCount - 1, &tempMemoryMapEntry);
	uintptr_t lastAddress       = tempMemoryMapEntry.Start + tempMemoryMapEntry.Size;
	uintptr_t lastUsableAddress = 0;
	for (size_t i = entryCount; i-- > 0;)
	{
		getter(userdata, i, &tempMemoryMapEntry);
		if (!(tempMemoryMapEntry.Type & PMMMemoryMapTypeUsable))
			continue;
		lastUsableAddress = tempMemoryMapEntry.Start + tempMemoryMapEntry.Size;
		break;
	}

	size_t pmmRequiredSize = sizeof(struct FreeLUTState) + ((lastUsableAddress + 32767) / 32768); // (32768 = 4 KiB page size * 8 pages per byte)
	pmmRequiredSize        = (pmmRequiredSize + 4095) & ~0xFFFUL;
	size_t pmmAllocatedIn  = ~0UL;
	for (size_t i = 0; i < entryCount; ++i)
	{
		getter(userdata, i, &tempMemoryMapEntry);
		if (!(tempMemoryMapEntry.Type & PMMMemoryMapTypeUsable))
			continue;
		if (tempMemoryMapEntry.Start == 0)
		{
			tempMemoryMapEntry.Start += 0x3000;
			tempMemoryMapEntry.Size  -= 0x3000;
		}
		if (tempMemoryMapEntry.Size <= pmmRequiredSize)
			continue;
		pmmAllocatedIn = i;
		g_FreeLUT      = (struct FreeLUTState*) tempMemoryMapEntry.Start;
		break;
	}
	if (pmmAllocatedIn >= entryCount)
	{
		printf("FreeLUT CRITICAL: Could not allocate PMM requiring %zu pages\n", pmmRequiredSize >> 12);
		KernelPanic();
	}

	g_FreeLUT->Stats = (struct PMMMemoryStats) {
		.Address             = g_FreeLUT,
		.Footprint           = pmmRequiredSize >> 12,
		.LastUsableAddress   = (void*) lastUsableAddress,
		.LastPhysicalAddress = (void*) lastAddress,
		.PagesTaken          = 0,
		.PagesFree           = 0,
		.AllocCalls          = 0,
		.FreeCalls           = 0
	};
	g_FreeLUT->MemoryMapCount = 0;
	g_FreeLUT->MemoryMap      = nullptr;
	g_FreeLUT->Bitmap         = (uint64_t*) ((uintptr_t) g_FreeLUT + sizeof(struct FreeLUTState));
	g_FreeLUT->Last           = nullptr;
	memset(g_FreeLUT->Bitmap, 0, lastUsableAddress >> 15);
	memset(g_FreeLUT->LUT, 0, sizeof(g_FreeLUT->LUT));

	for (size_t i = 0; i < entryCount; ++i)
	{
		getter(userdata, i, &tempMemoryMapEntry);
		if (tempMemoryMapEntry.Type != PMMMemoryMapTypeUsable)
			continue;

		if (tempMemoryMapEntry.Start == 0)
		{
			tempMemoryMapEntry.Start += 0x3000;
			tempMemoryMapEntry.Size  -= 0x3000;
		}
		if (pmmAllocatedIn == i)
		{
			tempMemoryMapEntry.Start += pmmRequiredSize;
			tempMemoryMapEntry.Size  -= pmmRequiredSize;
		}

		if (tempMemoryMapEntry.Size == 0)
			continue;
		FreeLUTPMMFree((void*) tempMemoryMapEntry.Start, tempMemoryMapEntry.Size >> 12);
		g_FreeLUT->Stats.PagesTaken += tempMemoryMapEntry.Size >> 12;
	}

	const size_t memoryMapPageCount                   = (entryCount + 3 * sizeof(struct PMMMemoryMapEntry) + 4095) >> 12;
	g_FreeLUT->MemoryMap                              = FreeLUTPMMAlloc(memoryMapPageCount, 12, (void*) -1);
	g_FreeLUT->Stats.Footprint                       += memoryMapPageCount;
	g_FreeLUT->MemoryMapCount                         = 0;
	g_FreeLUT->MemoryMap[g_FreeLUT->MemoryMapCount++] = (struct PMMMemoryMapEntry) {
		.Start = 0,
		.Size  = 0x1000,
		.Type  = PMMMemoryMapTypeNullGuard
	};
	g_FreeLUT->MemoryMap[g_FreeLUT->MemoryMapCount++] = (struct PMMMemoryMapEntry) {
		.Start = 0x1000,
		.Size  = 0x2000,
		.Type  = PMMMemoryMapTypeTrampoline
	};
	for (size_t i = 0; i < entryCount; ++i)
	{
		getter(userdata, i, &tempMemoryMapEntry);

		if (tempMemoryMapEntry.Start == 0)
		{
			tempMemoryMapEntry.Start += 0x3000;
			tempMemoryMapEntry.Size  -= 0x3000;
		}
		if (pmmAllocatedIn == i)
		{
			g_FreeLUT->MemoryMap[g_FreeLUT->MemoryMapCount++] = (struct PMMMemoryMapEntry) {
				.Start = tempMemoryMapEntry.Start,
				.Size  = pmmRequiredSize,
				.Type  = PMMMemoryMapTypePMM
			};
			tempMemoryMapEntry.Start += pmmRequiredSize;
			tempMemoryMapEntry.Size  -= pmmRequiredSize;
		}
		if (tempMemoryMapEntry.Size == 0)
			continue;
		if (tempMemoryMapEntry.Type == PMMMemoryMapTypeUsable)
			tempMemoryMapEntry.Type = PMMMemoryMapTypeTaken;
		g_FreeLUT->MemoryMap[g_FreeLUT->MemoryMapCount++] = (struct PMMMemoryMapEntry) {
			.Start = tempMemoryMapEntry.Start,
			.Size  = tempMemoryMapEntry.Size,
			.Type  = tempMemoryMapEntry.Type
		};
	}
}

void FreeLUTPMMReclaim(void)
{
	for (size_t i = 0; i < g_FreeLUT->MemoryMapCount; ++i)
	{
		struct PMMMemoryMapEntry* entry = &g_FreeLUT->MemoryMap[i];
		if (!(entry->Type & PMMMemoryMapTypeUsable))
			continue;

		FreeLUTPMMFree((void*) entry->Start, entry->Size >> 12);
		g_FreeLUT->Stats.PagesTaken += entry->Size >> 12;
		entry->Type                  = PMMMemoryMapTypeTaken;
	}

	size_t                    moveCount = 0;
	struct PMMMemoryMapEntry* pEntry    = &g_FreeLUT->MemoryMap[0];
	for (size_t i = 1; i < g_FreeLUT->MemoryMapCount; ++i)
	{
		struct PMMMemoryMapEntry* entry = &g_FreeLUT->MemoryMap[i];
		if (entry->Type == pEntry->Type &&
			entry->Start == pEntry->Start + pEntry->Size)
		{
			pEntry->Size += entry->Size;
			++moveCount;
		}
		else
		{
			g_FreeLUT->MemoryMap[i - moveCount] = *entry;
			pEntry                              = &g_FreeLUT->MemoryMap[i - moveCount];
		}
	}
	g_FreeLUT->MemoryMapCount -= moveCount;
}

void FreeLUTPMMGetMemoryStats(struct PMMMemoryStats* stats)
{
	if (!stats)
		return;

	FreeLUTLock();
	*stats = g_FreeLUT->Stats;
	FreeLUTUnlock();
}

size_t FreeLUTPMMGetMemoryMap(const struct PMMMemoryMapEntry** entries)
{
	if (!entries)
		return 0;

	FreeLUTLock();
	*entries     = g_FreeLUT->MemoryMap;
	size_t count = g_FreeLUT->MemoryMapCount;
	FreeLUTUnlock();
	return count;
}

void* FreeLUTPMMAlloc(size_t count, uint8_t alignment, void* largestAddress)
{
	if (count == 0 || !largestAddress)
		return nullptr;

	const size_t alignmentVal  = 1UL << (alignment - 12);
	const size_t alignmentMask = alignmentVal - 1;
	const size_t alignedCount  = count + alignmentMask;

	struct FreeLUTPageHeader* header = nullptr;
	if (largestAddress == (void*) -1)
		header = FreeLUTTakeFreeRange(alignedCount);
	else
		header = FreeLUTTakeFreeRangeBelow(alignedCount, largestAddress);
	if (!header)
	{
		if (alignment > 12)
		{
			if (largestAddress == (void*) -1)
				header = FreeLUTTakeFreeRangeAligned(count, alignment);
			else
				header = FreeLUTTakeFreeRangeAlignedBelow(count, alignment, largestAddress);
			if (header)
				goto ALLOCATE;
		}
		return nullptr;
	}

ALLOCATE:
	g_FreeLUT->Stats.PagesFree -= count;
	++g_FreeLUT->Stats.AllocCalls;
	const uintptr_t headerPage    = (uintptr_t) header >> 12;
	const uintptr_t lastRangePage = headerPage + header->Count - 1;
	const uintptr_t firstPage     = (headerPage + alignmentMask) & ~alignmentMask;
	const uintptr_t lastPage      = firstPage + count - 1;
	if (count > 1)
		FreeLUTBitmapSetRange(firstPage, lastPage, false);
	else
		FreeLUTBitmapSetEntry(firstPage, false);
	if (headerPage != firstPage)
	{
		FreeLUTFillFreePages(headerPage, firstPage - 1);
		FreeLUTInsertFreePage(header);
	}
	if (lastPage != lastRangePage)
	{
		FreeLUTFillFreePages(lastPage + 1, lastRangePage);
		FreeLUTInsertFreePage((struct FreeLUTPageHeader*) ((lastPage + 1) << 12));
	}
	return (void*) (firstPage << 12);
}

void FreeLUTPMMFree(void* address, size_t count)
{
	if (!address ||
		count == 0)
		return;

	const uintptr_t firstPage      = (uintptr_t) address >> 12;
	const uintptr_t lastPage       = firstPage + count - 1;
	const size_t    allocatedCount = FreeLUTBitmapCount(firstPage, lastPage, false);
	if (allocatedCount == 0)
		return;

	// TODO(MarcasRealAccount): Coalesce all free ranges already in the range, only needed when `allocatedCount != lastPage - firstPage`
	g_FreeLUT->Stats.PagesFree += allocatedCount;
	++g_FreeLUT->Stats.FreeCalls;
	if (count > 1)
		FreeLUTBitmapSetRange(firstPage, lastPage, true);
	else
		FreeLUTBitmapSetEntry(firstPage, true);

	uintptr_t bottomPage = firstPage;
	size_t    totalCount = count;
	if (FreeLUTBitmapGetEntry(firstPage - 1))
	{
		struct FreeLUTPageHeader* header = FreeLUTGetFirstPage((struct FreeLUTPageHeader*) ((firstPage - 1) << 12));
		bottomPage                       = (uintptr_t) header >> 12;
		totalCount                      += header->Count;
		FreeLUTEraseFreeRange(header);
	}
	if (FreeLUTBitmapGetEntry(lastPage + 1))
	{
		struct FreeLUTPageHeader* header = (struct FreeLUTPageHeader*) ((lastPage + 1) << 12);
		totalCount                      += header->Count;
		FreeLUTEraseFreeRange(header);
	}
	FreeLUTFillFreePages(bottomPage, bottomPage + totalCount - 1);
	FreeLUTInsertFreePage((struct FreeLUTPageHeader*) (bottomPage << 12));
}