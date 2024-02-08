# PMM

```c
enum PMMMemoryMapType
{
    PMMMemoryMapTypeInvalid           = 0x00,
	PMMMemoryMapTypeUsable            = 0x01,
	PMMMemoryMapTypeReclaimable       = 0x11,
	PMMMemoryMapTypeLoaderReclaimable = 0x21,
	PMMMemoryMapTypeTaken             = 0x02,
	PMMMemoryMapTypeNullGuard         = 0x12,
	PMMMemoryMapTypeTrampoline        = 0x22,
	PMMMemoryMapTypeKernel            = 0x04,
	PMMMemoryMapTypeModule            = 0x14,
	PMMMemoryMapTypePMM               = 0x24,
	PMMMemoryMapTypeReserved          = 0x08,
	PMMMemoryMapTypeACPI              = 0x18,
	PMMMemoryMapTypeNVS               = 0x28
};

struct PMMMemoryMapEntry
{
    uintptr_t Start;
    size_t    Size;

    enum PMMMemoryMapType Type;
};

struct PMMMemoryStats
{
    void*  Address;
    size_t Footprint;
    void*  LastUsableAddress;
    void*  LastPhysicalAddress;
    size_t PagesTaken;
    size_t PagesFree;
    size_t AllocCalls;
    size_t FreeCalls;
};

typedef bool (*PMMGetMemoryMapEntryFn)(void* userdata, size_t index, struct PMMMemoryMapEntry* entry);

struct PMMImplementation
{
    void   (*Init)(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata);
    void   (*Reclaim)(void);
    void   (*GetMemoryStats)(struct PMMMemoryStats* stats);
    size_t (*GetMemoryMap)(const struct PMMMemoryMapEntry** entries);
    void*  (*Alloc)(size_t count, uint8_t alignment, void* largestAddress);
    void   (*Free)(void* address, size_t count);
};
```

All valid implementations shall follow the following details.
- Fully thread safe using either mutexes or lockfree atomics.
- All callbacks in `PMMImplementation` need to be implemented.
    1. `void Init(size_t entryCount, PMMGetMemoryMapEntryFn getter, void* userdata)`
        - Initializes the PMM, frees the ranges that are exactly `PMMMemoryMapTypeUsable`, turning them into `PMMMemoryMapTypeTaken`, leaving reclaimable ranges reserved, however it shall reserve the first 16KiB of memory for use by other internal systems. First 4KiB shall be `PMMMemoryMapTypeNullGuard`, the next 12KiB shall be `PMMMemoryMapTypeTrampoline`.
        - Thread safety: None.
    2. `void Reclaim(void)`
        - Frees all remaining `type & PMMMemoryMapTypeUsable` ranges, turning them into `PMMMemoryMapTypeTaken`.
        - Thread safety: None.
    3. `void GetMemoryStats(struct PMMMemoryStats* stats)`
        - Gets the current memory statistics.
        - Thread safety: Thread safe without mutexes or lockfree atomics.
    4. `size_t GetMemoryMap(const struct PMMMemoryMapEntry** entries)`
        - Gets the current memory map, memory map shall not change after a `Reclaim()` call.
        - Thread safety: Thread safe without mutexes or lockfree atomics.
    5. `void* Alloc(size_t count, uint8_t alignment, void* largestAddress)`
        - Allocate `count` consecutive pages aligned to a `2^(alignment)` alignment below `largestAddress`.
        - Thread safety: Thread safe.
    6. `void Free(void* address, size_t count)`
        - Frees `count` consecutive pages starting at `address`.
        - Thread safety: Thread safe.

# VMM

```c
enum VMMAllocFlags
{
    VMMPageSize4KiB     = 0x0000'0000,
    VMMPageSize2MiB     = 0x0000'0001,
    VMMPageSize1GiB     = 0x0000'0002,
    VMMPageSizeReserved = 0x0000'0003,
    VMMPageSizeBits     = 0x0000'0003,

    VMMPageProtectReadWrite        = 0x0000'0000,
    VMMPageProtectReadOnly         = 0x0000'0004,
    VMMPageProtectReadWriteExecute = 0x0000'0008,
    VMMPageProtectReadExecute      = VMMPageProtectReadOnly | VMMPageProtectReadWriteExecute,
    VMMPageProtectBits             = 0x0000'000C,

    VMMPageCommit     = 0x0000'0010,
    VMMPageOptionBits = 0x0000'FFF0,

    VMMPageUnmapped     = 0x0000'0000,
    VMMPageMapped       = 0x0001'0000,
    VMMPageCommitted    = 0x0002'0000,
    VMMPageOnDisk       = 0x0010'0000,
    VMMPageInFile       = 0x0020'0000,
    VMMPagePagedOut     = VMMPageMapped | VMMPageOnDisk,
    VMMPageMappedToFile = VMMPageMapped | VMMPageInFile,
    VMMPageStatusBits   = 0xFFFF'0000
};

struct VMMRangeStats
{
    void*  Start;
    size_t Count;

    enum VMMAllocFlags Flags;
};

struct VMMMemoryStats
{
    size_t Footprint;

    size_t PagesAllocated;
    size_t PagesMapped;
    size_t PagesCommitted;
    size_t PagesPagedOut;

    size_t AllocCalls;
    size_t FreeCalls;
    size_t ProtectCalls;
    size_t MapCalls;
    size_t PageOutCalls;
};

struct VMMImplementation
{
    void* (*Create)(void);
    void  (*Destroy)(void* vmm);
    void  (*GetMemoryStats)(void* vmm, struct VMMMemoryStats* stats);
    void* (*GetRootTable)(void* vmm);
    void  (*FlushTable)(void* vmm);
    bool  (*Query)(void* vmm, void* virtualAddress, struct VMMRangeStats* stats);
    void* (*Translate)(void* vmm, void* virtualAddress);
    void* (*Alloc)(void* vmm, size_t count, uint8_t alignment, enum VMMAllocFlags flags);
    void* (*AllocAt)(void* vmm, void* virtualAddress, size_t count, uint8_t alignment, enum VMMAllocFlags flags);
    void  (*Free)(void* vmm, void* virtualAddress, size_t count);
    void  (*Protect)(void* vmm, void* virtualAddress, size_t count, enum VMMAllocFlags flags);
    void  (*Commit)(void* vmm, void* virtualAddress, size_t count, bool decommit);
    void  (*Map)(void* vmm, void* virtualAddress, void* physicalAddress, size_t count, bool unmap);
    void  (*MapToFile)(void* vmm, void* virtualAddress, struct File* file, size_t count, bool unmap);
    void  (*PageOut)(void* vmm, void* virtualAddress, size_t count, bool pageIn);
};
```

All valid implementations shall follow the following details.
- Fully thread safe using either mutexes or lockfree atomics.
- All callbacks in `VMMImplementation` need to be implemented.
    1. `void* Create(void)`
        - Creates a new instance with all physical, first and last 2MiB pages reserved.  
          Last usable 4KiB page should point to the instance.  
          All pages used by the instance should be memory mapped 1:1.  
          First value stored in the instance should be a pointer pointing to the implementation.
        - Thread safety: None.
    2. `void Destroy(void* vmm)`
        - Frees every committed, paged to disk and mapped to file page.  
          Frees all memory used by the instance.
        - Thread safety: None.
    3. `void GetMemoryStats(void* vmm, struct VMMMemoryStats* stats)`
        - Gets the current memory statistics.
        - Thread safety: Thread safe without mutexes or lockfree atomics.
    4. `void* GetRootTable(void* vmm)`
        - Gets the root page table physical address.
        - Thread safety: Thread safe without mutexes or lockfree atomics.
    5. `void FlushTable(void* vmm)`
        - Ensures the page table is fully up to date, should also invalidate the TLB.
        - Thread safety: Thread safe.
    6. `bool Query(void* vmm, void* virtualAddress, struct VMMRangeStats* stats)`
        - Queries information about the range at `virtualAddress`.  
          Returns true if the range exists, false otherwise.
        - Thread safety: Thread safe.
    7. `void* Translate(void* vmm, void* virtualAddress)`
        - Translates `virtualAddress` into its physical address or `nullptr` otherwise.
        - Thread safety: Thread safe.
    8. `void* Alloc(void* vmm, size_t count, uint8_t alignment, enum VMMAllocFlags flags)`
        - Allocate `count` consecutive pages aligned to a `2^(alignment)` alignment with the specified flags.
        - Thread safety: Thread safe.
    9. `void* AllocAt(void* vmm, void* virtualAddress, size_t count, uint8_t alignment, enum VMMAllocFlags flags)`
        - Allocate `count` consecutive pages starting at `virtualAddress` aligned to a `2^(alignment)` alignment with the specified flags.
        - Thread safety: Thread safe.
    10. `void Free(void* vmm, void* virtualAddress, size_t count)`
        - Frees `count` consecutive pages starting at `virtualAddress`.
        - Thread safety: Thread safe.
    11. `void Protect(void* vmm, void* virtualAddress, size_t count, enum VMMAllocFlags flags)`
        - Changes the protection of `count` consecutive pages starting at `virtualAddress`.
        - Thread safety: Thread safe.
    12. `void Commit(void* vmm, void* virtualAddress, size_t count, bool decommit)`
        - When `decommit == true` decommits `count` consecutive pages starting at `virtualAddress`.  
          Will free every committed pages.  
          Marks the range as non committed.
        - When `decommit == false` commits `count` consecutive pages starting at `virtualAddress`.  
          Will allocate every non committed pages.  
          Marks the range as committed.
        - Thread safety: Thread safe.
    13. `void Map(void* vmm, void* virtualAddress, void* physicalAddress, size_t count, bool unmap)`
        - When `unmap == true` unmaps `count` consecutive pages starting at `virtualAddress`.  
          Will free every committed or paged out pages.  
          Will release every files mapped.
          Marks the range as unmapped.
        - When `unmap == false` maps `count` consecutive pages starting at `virtualAddress` to `physicalAddress + 4096 * pageOffset`.  
          Marks the range as mapped.
        - Thread safety: Thread safe.
    14. `void MapToFile(void* vmm, void* virtualAddress, struct File* file, size_t count, bool unmap)`
        - When `unmap == true` unmaps `count` consecutive pages starting at `virtualAddress`.
          Will free every committed or paged out pages.
          Will release every files mapped.
          Marks the range as unmapped.
        - When `unmap == false` maps `count` consecutive pages starting at `virtualAddress` to the file handle `file`.  
          Marks the range as mapped to file.
        - Thread safety: Thread safe.
    15. `void PageOut(void* vmm, void* virtualAddress, size_t count, bool pageIn)`
        - When `pageIn == true` commits `count` consecutive pages starting at `virtualAddress` and copies the contents of the pages stored in the pagefile.  
          Will allocate every non committed pages.
          Marks the range as committed and not paged out.
        - When `pageIn == false` allocates `count` consecutive pages starting at `virtualAddress` in the pagefile and copies the contents of the committed pages to the pagefile.  
          Will not free any committed pages.  
          Marks the range as paged out.
        - Thread safety: Thread safe.

# Fonts

