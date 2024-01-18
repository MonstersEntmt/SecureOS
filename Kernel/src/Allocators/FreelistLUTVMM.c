#define VMM_USE_FREELIST_LUT 1

// TODO(MarcasRealAccount): Implement allocator selection as a runtime option through the commandline
#if VMM_USE_FREELIST_LUT

	#include "VMM.h"
	#include "PMM.h"

	#include <string.h>

struct VMMFreeEntry
{
	uint64_t             Start;
	uint64_t             Count;
	struct VMMFreeEntry* Prev;
	struct VMMFreeEntry* Next;
};

struct VMMFreePage
{
	uint64_t            Bitmap[2];
	struct VMMFreePage* PrevFreePage;
	struct VMMFreePage* NextFreePage;
	struct VMMFreeEntry Entries[127];
};

struct VMMState
{
	struct VMMMemoryStats Stats;

	uint8_t   Levels;
	bool      Supports1GiB;
	uint64_t* PageTableRoot;
	uint64_t* FreeTableRoot;

	struct VMMFreePage* FirstFreePage;
	struct VMMFreePage* LastFreePage;

	struct VMMFreeEntry* Last;
	struct VMMFreeEntry* LUT[255];
};

extern uint64_t  VMMArchConstructPageTableEntry(uint64_t physicalAddress, enum VMMPageType type, enum VMMPageProtect protect);
extern uint64_t  VMMArchConstructPageTablePointer(uint64_t* subTableAddress);
extern void      VMMArchGetPageTableEntry(uint64_t entry, uint8_t level, uint64_t* physicalAddress, enum VMMPageType* type, enum VMMPageProtect* protect);
extern uint64_t* VMMArchGetPageTablePointer(uint64_t entry);
extern void      VMMArchActivate(uint64_t* pageTableRoot, uint8_t levels, bool use1GiB);

static uint64_t VMMGetLUTValue(uint8_t index)
{
	if (index < 192)
		return (uint64_t) index + 1;
	return (1UL << (index - 191)) + 192;
}

static uint8_t VMMGetLUTIndex(uint64_t value)
{
	if (value < 193)
		return (uint8_t) value - 1;
	return (254 - __builtin_clzll(value - 192));
}

static uint8_t VMMGetLUTCeilIndex(uint64_t value)
{
	if (value < 193)
		return (uint8_t) value - 1;
	return (255 - __builtin_clzll(value - 193));
}

static void VMMPageTableFreeRecursively(struct VMMState* state, uint64_t* pageTable, uint64_t* freeTable, uint8_t level)
{
	for (uint16_t i = 0; i < 512; ++i)
	{
		uint64_t freeEntry = freeTable[i];
		switch (freeEntry)
		{
		case 0b00: break;
		case 0b01:
			VMMPageTableFreeRecursively(state, (uint64_t*) (pageTable[i] & 0xF'FFFF'FFFF'F000UL), (uint64_t*) (freeEntry & 0xF'FFFF'FFFF'F000UL), level - 1);
			break;
		case 0b10:
		case 0b11: break;
		}
	}
	PMMFree(pageTable, 1);
	PMMFree(freeTable, 1);
	state->Stats.AllocatorFootprint -= 8192;
}

static void VMMPageTableMap(struct VMMState* state, uint64_t page, uint64_t physicalAddress)
{
	uint64_t* pageTable = state->PageTableRoot;
	uint64_t* freeTable = state->FreeTableRoot;
	for (uint8_t i = state->Levels; i-- > 0;)
	{
		uint16_t entry     = (page >> (9 * i)) & 511;
		uint64_t freeEntry = freeTable[entry];
		switch (freeEntry & 3)
		{
		case 0b00: return;
		case 0b01:
			pageTable = VMMArchGetPageTablePointer(pageTable[entry]);
			freeTable = (uint64_t*) (freeEntry & 0xF'FFFF'FFFF'F000UL);
			break;
		case 0b10:
		case 0b11:
			enum VMMPageType    type;
			enum VMMPageProtect protect;
			VMMArchGetPageTableEntry(pageTable[entry], i, nullptr, &type, &protect);
			pageTable[i] = VMMArchConstructPageTableEntry(physicalAddress, type, protect);
			return;
		}
	}
}

static uint64_t VMMPageTableMapLinearRecursive(struct VMMState* state, uint64_t* pageTable, uint64_t* freeTable, uint64_t firstPage, uint64_t lastPage, uint64_t physicalAddress, uint8_t level)
{
	uint16_t firstEntry = (firstPage >> (9 * level)) & 511;
	uint16_t lastEntry  = (lastPage >> (9 * level)) & 511;
	for (uint16_t i = firstEntry; i <= lastEntry; ++i)
	{
		uint64_t freeEntry = freeTable[i];
		switch (freeEntry & 3)
		{
		case 0b00: break;
		case 0b01:
		{
			uint64_t firstSubPage = i == firstEntry ? firstPage - (i << (9 * level)) : 0;
			uint64_t lastSubPage  = i != lastEntry ? (1 << (9 * level)) - 1 : lastPage - (i << (9 * level));
			physicalAddress       = VMMPageTableMapLinearRecursive(state, VMMArchGetPageTablePointer(pageTable[i]), (uint64_t*) (freeEntry & 0xF'FFFF'FFFF'F0UL), firstSubPage, lastSubPage, physicalAddress, level - 1);
			break;
		}
		case 0b10:
		case 0b11:
		{
			enum VMMPageType    type;
			enum VMMPageProtect protect;
			VMMArchGetPageTableEntry(pageTable[i], level, nullptr, &type, &protect);
			pageTable[i]     = VMMArchConstructPageTableEntry(physicalAddress, type, protect);
			physicalAddress += 4096 << (9 * level);
			break;
		}
		}
	}
	return physicalAddress;
}

static void VMMPageTableMapLinear(struct VMMState* state, uint64_t firstPage, uint64_t lastPage, uint64_t physicalAddress)
{
	VMMPageTableMapLinearRecursive(state, state->PageTableRoot, state->FreeTableRoot, firstPage, lastPage, physicalAddress, state->Levels - 1);
}

static void VMMPageTableFillProtectRecursive(struct VMMState* state, uint64_t* pageTable, uint64_t* freeTable, uint64_t firstPage, uint64_t lastPage, enum VMMPageProtect protect, uint8_t level)
{
	uint16_t firstEntry = (firstPage >> (9 * level)) & 511;
	uint16_t lastEntry  = (lastPage >> (9 * level)) & 511;
	for (uint16_t i = firstEntry; i <= lastEntry; ++i)
	{
		uint64_t freeEntry = freeTable[i];
		switch (freeEntry & 3)
		{
		case 0b00: break;
		case 0b01:
		{
			uint64_t firstSubPage = i == firstEntry ? firstPage - (i << (9 * level)) : 0;
			uint64_t lastSubPage  = i != lastEntry ? (1 << (9 * level)) - 1 : lastPage - (i << (9 * level));
			VMMPageTableFillProtectRecursive(state, VMMArchGetPageTablePointer(pageTable[i]), (uint64_t*) (freeEntry & 0xF'FFFF'FFFF'F0UL), firstSubPage, lastSubPage, protect, level - 1);
			break;
		}
		case 0b10:
		case 0b11:
		{
			uint64_t         physicalAddress;
			enum VMMPageType type;
			VMMArchGetPageTableEntry(pageTable[i], level, &physicalAddress, &type, nullptr);
			pageTable[i] = VMMArchConstructPageTableEntry(physicalAddress, type, protect);
			break;
		}
		}
	}
}

static void VMMPageTableFillProtect(struct VMMState* state, uint64_t firstPage, uint64_t lastPage, enum VMMPageProtect protect)
{
	VMMPageTableFillProtectRecursive(state, state->PageTableRoot, state->FreeTableRoot, firstPage, lastPage, protect, state->Levels - 1);
}

static void VMMPageTableFillUsedRecursive(struct VMMState* state, uint64_t* pageTable, uint64_t* freeTable, uint64_t firstPage, uint64_t lastPage, enum VMMPageType type, enum VMMPageProtect protect, uint8_t level)
{
	uint8_t minLevel = 0;
	switch (type)
	{
	case VMM_PAGE_TYPE_4KIB: minLevel = 0; break;
	case VMM_PAGE_TYPE_2MIB: minLevel = 1; break;
	case VMM_PAGE_TYPE_1GIB: minLevel = 2; break;
	}

	uint16_t firstEntry = (firstPage >> (9 * level)) & 511;
	uint16_t lastEntry  = (lastPage >> (9 * level)) & 511;
	if (level == minLevel)
	{
		for (uint16_t i = firstEntry; i <= lastEntry; ++i)
		{
			pageTable[i] = VMMArchConstructPageTableEntry(0, type, protect);
			freeTable[i] = 3; // TODO(MarcasRealAccount): Perhaps store allocated page information?
		}
	}
	else
	{
		for (uint16_t i = firstEntry; i <= lastEntry; ++i)
		{
			uint64_t  freeEntry     = freeTable[i];
			uint64_t* nextPageTable = nullptr;
			uint64_t* nextFreeTable = nullptr;
			if (freeEntry & 1)
			{
				nextPageTable = VMMArchGetPageTablePointer(pageTable[i]);
				nextFreeTable = (uint64_t*) (freeTable[i] & 0xF'FFFF'FFFF'F000UL);
			}
			else
			{
				void* tablePages = PMMAlloc(2);
				if (tablePages)
				{
					memset(tablePages, 0, 8192);

					nextPageTable = (uint64_t*) tablePages;
					nextFreeTable = (uint64_t*) tablePages + 512;
					pageTable[i]  = VMMArchConstructPageTablePointer(nextPageTable);
					freeTable[i]  = 1 | ((uint64_t) nextFreeTable & 0xF'FFFF'FFFF'F000UL);
				}
				else
				{
					nextPageTable = (uint64_t*) PMMAlloc(1);
					if (!nextPageTable)
					{
						// TODO(MarcasRealAccount): PANIC
						return;
					}
					nextFreeTable = (uint64_t*) PMMAlloc(1);
					if (!nextFreeTable)
					{
						PMMFree(nextPageTable, 1);
						// TODO(MarcasRealAccount): PANIC
						return;
					}
					memset(nextPageTable, 0, 4096);
					memset(nextFreeTable, 0, 4096);

					pageTable[i] = VMMArchConstructPageTablePointer(nextPageTable);
					freeTable[i] = 1 | ((uint64_t) nextFreeTable & 0xF'FFFF'FFFF'F000UL);
				}
				state->Stats.AllocatorFootprint += 8192;
			}

			uint64_t firstSubPage = i == firstEntry ? firstPage - (i << (9 * level)) : 0;
			uint64_t lastSubPage  = i != lastEntry ? (1 << (9 * level)) - 1 : lastPage - (i << (9 * level));
			VMMPageTableFillUsedRecursive(state, nextPageTable, nextFreeTable, firstSubPage, lastSubPage, type, protect, level - 1);
		}
	}
}

static void VMMPageTableFillUsed(struct VMMState* state, uint64_t firstPage, uint64_t lastPage, enum VMMPageType type, enum VMMPageProtect protect)
{
	VMMPageTableFillUsedRecursive(state, state->PageTableRoot, state->FreeTableRoot, firstPage, lastPage, type, protect, state->Levels - 1);
}

static void VMMPageTableFillFree(struct VMMState* state, struct VMMFreeEntry* entry)
{
	uint64_t* firstPageTable = state->PageTableRoot;
	uint64_t* firstFreeTable = state->FreeTableRoot;
	uint64_t* lastPageTable  = state->PageTableRoot;
	uint64_t* lastFreeTable  = state->FreeTableRoot;

	uint64_t firstPage = entry->Start;
	uint64_t lastPage  = firstPage + entry->Count - 1;
	for (uint8_t level = state->Levels; level-- > 0;)
	{
		uint16_t firstEntry    = (firstPage >> (9 * level)) & 511;
		uint16_t lastEntry     = (lastPage >> (9 * level)) & 511;
		uint64_t remMask       = ~0UL >> (64 - (9 * level));
		uint64_t remFirstEntry = firstPage & remMask;
		uint64_t remLastEntry  = lastPage & remMask;

		int16_t layerFillStart = remFirstEntry == 0 ? firstEntry : firstEntry + 1;
		int16_t layerFillEnd   = remFirstEntry == remMask ? lastEntry : lastEntry - 1;

		if (firstFreeTable == lastFreeTable)
		{
			if (!firstFreeTable)
				break;

			for (int16_t i = layerFillStart; i <= layerFillEnd; ++i)
			{
				uint64_t freeEntry = firstFreeTable[i];
				if ((freeEntry & 3) == 1)
					VMMPageTableFreeRecursively(state, VMMArchGetPageTablePointer(firstPageTable[i]), (uint64_t*) (freeEntry & 0xF'FFFF'FFFF'F000UL), level - 1);
				firstPageTable[i] = 0;
				firstFreeTable[i] = (uint64_t) entry & 0xF'FFFF'FFFF'FFE0UL;
			}
		}
		else
		{
			if (firstFreeTable)
			{
				for (int16_t i = layerFillStart; i < 512; ++i)
				{
					uint64_t freeEntry = firstFreeTable[i];
					if ((freeEntry & 3) == 1)
						VMMPageTableFreeRecursively(state, VMMArchGetPageTablePointer(firstPageTable[i]), (uint64_t*) (freeEntry & 0xF'FFFF'FFFF'F000UL), level - 1);
					firstPageTable[i] = 0;
					firstFreeTable[i] = (uint64_t) entry & 0xF'FFFF'FFFF'FFE0UL;
				}
			}
			if (lastFreeTable)
			{
				for (int16_t i = 0; i <= layerFillEnd; ++i)
				{
					uint64_t freeEntry = lastFreeTable[i];
					if ((freeEntry & 3) == 1)
						VMMPageTableFreeRecursively(state, VMMArchGetPageTablePointer(lastPageTable[i]), (uint64_t*) (freeEntry & 0xF'FFFF'FFFF'F000UL), level - 1);
					lastPageTable[i] = 0;
					lastFreeTable[i] = (uint64_t) entry & 0xF'FFFF'FFFF'FFE0UL;
				}
			}
		}

		if (firstFreeTable)
		{
			if (layerFillStart == firstEntry)
			{
				firstPageTable = nullptr;
				firstFreeTable = nullptr;
			}
			else
			{
				uint64_t firstFreeEntry = firstFreeTable[firstEntry];
				switch (firstFreeEntry & 3)
				{
				case 0b00:
					firstFreeTable[firstEntry] = (uint64_t) entry & 0xF'FFFF'FFFF'FFE0UL;
					firstPageTable             = nullptr;
					firstFreeTable             = nullptr;
					break;
				case 0b01:
					firstPageTable = VMMArchGetPageTablePointer(firstPageTable[firstEntry]);
					firstFreeTable = (uint64_t*) (firstFreeEntry & 0xF'FFFF'FFFF'F000UL);
					break;
				case 0b10:
				case 0b11:
					firstPageTable[firstEntry] = 0;
					firstFreeTable[firstEntry] = (uint64_t) entry & 0xF'FFFF'FFFF'FFE0UL;
					firstPageTable             = nullptr;
					firstFreeTable             = nullptr;
					break;
				}
			}
		}
		if (lastFreeTable)
		{
			if (layerFillEnd == lastEntry)
			{
				lastPageTable = nullptr;
				lastFreeTable = nullptr;
			}
			else
			{
				uint64_t lastFreeEntry = lastFreeTable[lastEntry];
				switch (lastFreeEntry & 3)
				{
				case 0b00:
					lastFreeTable[lastEntry] = (uint64_t) entry & 0xF'FFFF'FFFF'FFE0UL;
					lastPageTable            = nullptr;
					lastFreeTable            = nullptr;
					break;
				case 0b01:
					lastPageTable = VMMArchGetPageTablePointer(lastPageTable[lastEntry]);
					lastFreeTable = (uint64_t*) (lastFreeEntry & 0xF'FFFF'FFFF'F000UL);
					break;
				case 0b10:
				case 0b11:
					lastPageTable[lastEntry] = 0;
					lastFreeTable[lastEntry] = (uint64_t) entry & 0xF'FFFF'FFFF'FFE0UL;
					lastPageTable            = nullptr;
					lastFreeTable            = nullptr;
					break;
				}
			}
		}
	}
}

static uint8_t VMMFreePageGetFree(struct VMMFreePage* freePage)
{
	if (freePage->Bitmap[0] == 0)
	{
		if (freePage->Bitmap[1] == 0)
			return 128;
		else
			return __builtin_clzll(freePage->Bitmap[1]);
	}
	else
	{
		return __builtin_clzll(freePage->Bitmap[0]);
	}
}

static struct VMMFreeEntry* VMMFreeEntryNew(struct VMMState* state)
{
	if (!state->FirstFreePage)
	{
		state->FirstFreePage = (struct VMMFreePage*) PMMAlloc(1);
		state->LastFreePage  = state->FirstFreePage;
		memset(state->FirstFreePage, 0, 4096);
		state->FirstFreePage->Bitmap[0]  = ~0UL;
		state->FirstFreePage->Bitmap[1]  = ~0UL;
		state->Stats.AllocatorFootprint += 4096;
	}
	struct VMMFreePage* freePage  = state->FirstFreePage;
	uint8_t             freeEntry = VMMFreePageGetFree(freePage);
	if (freeEntry == 128)
	{
		freePage = (struct VMMFreePage*) PMMAlloc(1);
		memset(freePage, 0, 4096);
		freePage->Bitmap[0]                = ~0UL;
		freePage->Bitmap[1]                = ~0UL;
		freePage->NextFreePage             = state->FirstFreePage;
		state->FirstFreePage->PrevFreePage = freePage;
		state->FirstFreePage               = freePage;
		freeEntry                          = 0;
		state->Stats.AllocatorFootprint   += 4096;
	}

	freePage->Bitmap[freeEntry / 64] |= 1UL << (freeEntry & 63);
	struct VMMFreeEntry* entry        = &freePage->Entries[freeEntry];

	freeEntry = VMMFreePageGetFree(freePage);
	if (freeEntry == 128)
	{
		if (freePage->PrevFreePage)
			freePage->PrevFreePage->NextFreePage = freePage->NextFreePage;
		if (freePage->NextFreePage)
			freePage->NextFreePage->PrevFreePage = freePage->PrevFreePage;
		if (state->LastFreePage != freePage)
		{
			state->LastFreePage->NextFreePage = freePage;
			freePage->PrevFreePage            = state->LastFreePage;
		}
		freePage->NextFreePage = nullptr;
		state->LastFreePage    = freePage;
	}
	return entry;
}

static void VMMFreeEntryFree(struct VMMState* state, struct VMMFreeEntry* entry)
{
	struct VMMFreePage* freePage      = (struct VMMFreePage*) ((uint64_t) entry & ~0xFFFUL);
	uint8_t             freeEntry     = (uint8_t) (entry - freePage->Entries);
	freePage->Bitmap[freeEntry / 64] &= ~(1UL << (freeEntry & 63));

	if (state->LastFreePage == freePage)
		state->LastFreePage = freePage->PrevFreePage;
	if (state->FirstFreePage == freePage)
		state->FirstFreePage = freePage->NextFreePage;
	if (freePage->PrevFreePage)
		freePage->PrevFreePage->NextFreePage = freePage->NextFreePage;
	if (freePage->NextFreePage)
		freePage->NextFreePage->PrevFreePage = freePage->PrevFreePage;

	if (freePage->Bitmap[0] != 0 || freePage->Bitmap[1] != 0)
	{
		freePage->PrevFreePage = nullptr;
		freePage->NextFreePage = state->FirstFreePage;
		if (state->FirstFreePage)
			state->FirstFreePage->PrevFreePage = freePage;
		state->FirstFreePage = freePage;
	}
	else
	{
		PMMFree(freePage, 1);
		state->Stats.AllocatorFootprint -= 4096;
	}
}

static struct VMMFreeEntry* VMMInsertFreeRange(struct VMMState* state, uint64_t firstPage, uint64_t lastPage)
{
	struct VMMFreeEntry* entry = VMMFreeEntryNew(state);
	entry->Start               = firstPage;
	entry->Count               = lastPage - firstPage + 1;
	uint8_t index              = VMMGetLUTIndex(entry->Count);
	if (state->LUT[index])
	{
		struct VMMFreeEntry* other = state->LUT[index];
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

static void VMMEraseFreeRange(struct VMMState* state, struct VMMFreeEntry* entry)
{
	if (state->Last == entry)
		state->Last = entry->Prev;
	uint8_t index = VMMGetLUTIndex(entry->Count);
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
	VMMFreeEntryFree(state, entry);
}

static struct VMMFreeEntry* VMMGetFreeRange(struct VMMState* state, uint64_t count)
{
	uint8_t index = VMMGetLUTCeilIndex(count);
	if (state->LUT[index])
		return state->LUT[index];

	if (index == 0 ||
		VMMGetLUTValue(index) == count)
		return nullptr;

	struct VMMFreeEntry* cur = state->LUT[index - 1];
	while (cur && cur->Count < count)
		cur = cur->Next;
	return cur;
}

static struct VMMFreeEntry* VMMGetFreeRangeAt(struct VMMState* state, uint64_t page, uint64_t count)
{
	uint64_t* freeTable = state->FreeTableRoot;
	for (uint8_t i = state->Levels; i-- > 0;)
	{
		uint64_t freeEntry = freeTable[(page >> (9 * i)) & 511];
		if (freeEntry & 1)
		{
			freeTable = (uint64_t*) (freeEntry & 0xF'FFFF'FFFF'F000UL);
			if (!freeTable)
				return nullptr;
		}
		else
		{
			struct VMMFreeEntry* entry = (struct VMMFreeEntry*) (freeEntry & 0xF'FFFF'FFFF'FFE0UL);
			return entry && (entry->Start + entry->Count >= page + count) ? entry : nullptr;
		}
	}
	return nullptr;
}

static struct VMMFreeEntry* VMMGetAlignedRange(struct VMMState* state, uint64_t count, uint8_t alignment)
{
	uint8_t              index = VMMGetLUTIndex(count);
	struct VMMFreeEntry* cur   = state->LUT[index];

	uint64_t alignmentVal  = 1UL << (alignment - 12);
	uint64_t alignmentMask = alignmentVal - 1;
	while (cur && ((cur->Start + alignmentMask) & ~alignmentMask) > (cur->Start + cur->Count))
		cur = cur->Next;
	return cur;
}

void* VMMNewPageTable(void)
{
	struct VMMState* state = (struct VMMState*) PMMAlloc(3);
	if (!state)
		return nullptr;

	memset(state, 0, 3 * 4096);
	state->Stats = (struct VMMMemoryStats) {
		.AllocatorFootprint = 3 * 4096,
		.PagesAllocated     = 0
	};
	state->Levels              = 4; // TODO(MarcasRealAccount): Determine max levels and 1 GiB support
	state->Supports1GiB        = false;
	state->PageTableRoot       = (uint64_t*) state + 512;
	state->FreeTableRoot       = (uint64_t*) state + 1024;
	struct VMMFreeEntry* entry = VMMInsertFreeRange(state, 1, 0xF'FFFF'FFFE);
	VMMPageTableFillFree(state, entry);
	return state;
}

void VMMFreePageTable(void* pageTable)
{
	if (!pageTable)
		return;
	struct VMMState* state = (struct VMMState*) pageTable;

	VMMPageTableFreeRecursively(state, state->PageTableRoot, state->FreeTableRoot, state->Levels - 1);
	struct VMMFreePage* curFreePage = state->FirstFreePage;
	while (curFreePage)
	{
		struct VMMFreePage* nextFreePage = curFreePage->NextFreePage;
		PMMFree(curFreePage, 1);
		curFreePage = nextFreePage;
	}
	PMMFree(state, 1);
}

void VMMGetMemoryStats(void* pageTable, struct VMMMemoryStats* stats)
{
	if (!pageTable || !stats)
		return;

	struct VMMState* state = (struct VMMState*) pageTable;
	*stats                 = state->Stats;
}

void* VMMAlloc(void* pageTable, size_t count, uint8_t alignment, enum VMMPageType type, enum VMMPageProtect protect)
{
	if (!pageTable || count == 0)
		return nullptr;

	struct VMMState* state = (struct VMMState*) pageTable;

	switch (type)
	{
	case VMM_PAGE_TYPE_4KIB: alignment = alignment < 12 ? 12 : alignment; break;
	case VMM_PAGE_TYPE_2MIB: alignment = alignment < 21 ? 21 : alignment; break;
	case VMM_PAGE_TYPE_1GIB:
		alignment = alignment < 30 ? 30 : alignment;
		if (!state->Supports1GiB)
			type = VMM_PAGE_TYPE_2MIB;
		break;
	}

	uint64_t alignmentVal  = 1UL << (alignment - 12);
	uint64_t alignmentMask = alignmentVal - 1;

	struct VMMFreeEntry* entry = VMMGetFreeRange(state, count + alignmentVal);
	if (!entry)
	{
		entry = VMMGetAlignedRange(state, count, alignment);
		if (!entry)
			return nullptr;
	}

	state->Stats.PagesAllocated += count;
	uint64_t entryPage           = entry->Start;
	uint64_t lastRangePage       = entryPage + entry->Count - 1;
	uint64_t firstPage           = (entryPage + alignmentMask) & ~alignmentMask;
	uint64_t lastPage            = firstPage + count - 1;

	VMMEraseFreeRange(state, entry);
	VMMPageTableFillUsed(state, firstPage, lastPage, type, protect);
	if (entryPage != firstPage)
	{
		struct VMMFreeEntry* firstEntry = VMMInsertFreeRange(state, entryPage, firstPage - 1);
		VMMPageTableFillFree(state, firstEntry);
	}
	if (lastPage != lastRangePage)
	{
		struct VMMFreeEntry* lastEntry = VMMInsertFreeRange(state, lastPage + 1, lastRangePage);
		VMMPageTableFillFree(state, lastEntry);
	}
	return (void*) (firstPage * 4096);
}

void* VMMAllocAt(void* pageTable, uint64_t virtualAddress, size_t count, enum VMMPageType type, enum VMMPageProtect protect)
{
	if (!pageTable || count == 0)
		return nullptr;

	uint64_t firstPage = virtualAddress / 4096;

	struct VMMState* state = (struct VMMState*) pageTable;
	if (type == VMM_PAGE_TYPE_1GIB && !state->Supports1GiB)
		type = VMM_PAGE_TYPE_2MIB;
	struct VMMFreeEntry* entry = VMMGetFreeRangeAt(state, firstPage, count);
	if (!entry)
		return nullptr;

	state->Stats.PagesAllocated += count;
	uint64_t entryPage           = entry->Start;
	uint64_t lastRangePage       = entryPage + entry->Count - 1;
	uint64_t lastPage            = firstPage + count - 1;

	VMMEraseFreeRange(state, entry);
	VMMPageTableFillUsed(state, firstPage, lastPage, type, protect);
	if (entryPage != firstPage)
	{
		struct VMMFreeEntry* firstEntry = VMMInsertFreeRange(state, entryPage, firstPage - 1);
		VMMPageTableFillFree(state, firstEntry);
	}
	if (lastPage != lastRangePage)
	{
		struct VMMFreeEntry* lastEntry = VMMInsertFreeRange(state, lastPage + 1, lastRangePage);
		VMMPageTableFillFree(state, lastEntry);
	}
	return (void*) (firstPage * 4096);
}

void VMMFree(void* pageTable, void* virtualAddress, size_t count)
{
	if (!pageTable)
		return;

	uint64_t firstPage = (uint64_t) virtualAddress / 4096;

	struct VMMState* state = (struct VMMState*) pageTable;
	if (VMMGetFreeRangeAt(state, firstPage, 1))
		return;

	state->Stats.PagesAllocated -= count;

	uint64_t bottomPage = firstPage;
	uint64_t totalCount = count;

	struct VMMFreeEntry* entryBelow = VMMGetFreeRangeAt(state, firstPage - 1, 1);
	struct VMMFreeEntry* entryAbove = VMMGetFreeRangeAt(state, firstPage + count, 1);
	if (entryBelow)
	{
		bottomPage  = entryBelow->Start;
		totalCount += entryBelow->Count;
		VMMEraseFreeRange(state, entryBelow);
	}
	if (entryAbove)
	{
		totalCount += entryAbove->Count;
		VMMEraseFreeRange(state, entryAbove);
	}
	struct VMMFreeEntry* entry = VMMInsertFreeRange(state, bottomPage, bottomPage + totalCount - 1);
	VMMPageTableFillFree(state, entry);
}

void VMMProtect(void* pageTable, void* virtualAddress, size_t count, enum VMMPageProtect protect)
{
	if (!pageTable)
		return;

	uint64_t firstPage = (uint64_t) virtualAddress / 4096;
	uint64_t lastPage  = firstPage + count - 1;
	VMMPageTableFillProtect((struct VMMState*) pageTable, firstPage, lastPage, protect);
}

void VMMMap(void* pageTable, void* virtualAddress, void* physicalAddress)
{
	if (!pageTable)
		return;

	VMMPageTableMap((struct VMMState*) pageTable, (uint64_t) virtualAddress / 4096, (uint64_t) physicalAddress);
}

void VMMMapLinear(void* pageTable, void* virtualAddress, void* physicalAddress, size_t count)
{
	if (!pageTable)
		return;

	uint64_t firstPage = (uint64_t) virtualAddress / 4096;
	uint64_t lastPage  = firstPage + count - 1;
	VMMPageTableMapLinear((struct VMMState*) pageTable, firstPage, lastPage, (uint64_t) physicalAddress);
}

void VMMActivate(void* pageTable)
{
	if (!pageTable)
		return;

	struct VMMState* state = (struct VMMState*) pageTable;
	VMMArchActivate(state->PageTableRoot, state->Levels, state->Supports1GiB);
}

#endif