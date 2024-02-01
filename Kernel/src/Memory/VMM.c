#include "Memory/VMM.h"
#include "Memory/FreeLUT/FreeLUTVMM.h"

#include <string.h>

struct VMMImplementation
{
	const char* Name;
	void* (*Create)(void);
	void (*Destroy)(void* vmm);
	void (*GetMemoryStats)(void* vmm, struct VMMMemoryStats* stats);
	void* (*Alloc)(void* vmm, size_t count, uint8_t alignment, uint32_t flags);
	void* (*AllocAt)(void* vmm, void* virtualAddress, size_t count, uint32_t flags);
	void (*Free)(void* vmm, void* virtualAddress, size_t count);
	void (*Protect)(void* vmm, void* virtualAddress, size_t count, uint32_t flags);
	void (*Map)(void* vmm, void* virtualAddress, void* physicalAddress);
	void (*MapLinear)(void* vmm, void* virtualAddress, void* physicalAddress, size_t count);
	void* (*Translate)(void* vmm, void* virtualAddress);
	void* (*GetRootTable)(void* vmm, uint8_t* levels, bool* use1GiB);
};

static const struct VMMImplementation s_VMMImplementations[] = {
	(struct VMMImplementation) {
								.Name           = "freelut",
								.Create         = &FreeLUTVMMCreate,
								.Destroy        = &FreeLUTVMMDestroy,
								.GetMemoryStats = &FreeLUTVMMGetMemoryStats,
								.Alloc          = &FreeLUTVMMAlloc,
								.AllocAt        = &FreeLUTVMMAllocAt,
								.Free           = &FreeLUTVMMFree,
								.Protect        = &FreeLUTVMMProtect,
								.Map            = &FreeLUTVMMMap,
								.MapLinear      = &FreeLUTVMMMapLinear,
								.Translate      = &FreeLUTVMMTranslate,
								.GetRootTable   = &FreeLUTVMMGetRootTable,
								}
};

static const struct VMMImplementation* s_VMMImpl = nullptr;

void VMMSelect(const char* name, size_t nameLen)
{
	for (size_t i = 0; !s_VMMImpl && i < sizeof(s_VMMImplementations) / sizeof(*s_VMMImplementations); ++i)
	{
		size_t implLen = strlen(s_VMMImplementations[i].Name);
		if (nameLen != implLen)
			continue;
		if (memcmp(name, s_VMMImplementations[i].Name, nameLen) == 0)
			s_VMMImpl = &s_VMMImplementations[i];
	}
	if (!s_VMMImpl)
		s_VMMImpl = &s_VMMImplementations[0];
}

const char* VMMGetSelectedName(void)
{
	return s_VMMImpl->Name;
}

void* VMMCreate(void)
{
	return s_VMMImpl->Create();
}

void VMMDestroy(void* vmm)
{
	if (!vmm)
		return;
	s_VMMImpl->Destroy(vmm);
}

void VMMGetMemoryStats(void* vmm, struct VMMMemoryStats* stats)
{
	if (!vmm || !stats)
		return;
	s_VMMImpl->GetMemoryStats(vmm, stats);
}

void* VMMAlloc(void* vmm, size_t count, uint8_t alignment, uint32_t flags)
{
	if (!vmm)
		return nullptr;
	uint8_t minAlignment = 0;
	switch (flags & VMM_PAGE_SIZE_BITS)
	{
	case VMM_PAGE_SIZE_4KiB: minAlignment = 12; break;
	case VMM_PAGE_SIZE_2MiB: minAlignment = 21; break;
	case VMM_PAGE_SIZE_1GiB: minAlignment = 30; break;
	}
	return s_VMMImpl->Alloc(vmm, count, alignment < minAlignment ? minAlignment : alignment, flags);
}

void* VMMAllocAt(void* vmm, void* virtualAddress, size_t count, uint32_t flags)
{
	if (!vmm)
		return nullptr;
	return s_VMMImpl->AllocAt(vmm, virtualAddress, count, flags);
}

void VMMFree(void* vmm, void* virtualAddress, size_t count)
{
	if (!vmm)
		return;
	s_VMMImpl->Free(vmm, virtualAddress, count);
}

void VMMProtect(void* vmm, void* virtualAddress, size_t count, uint32_t protect)
{
	if (!vmm)
		return;
	s_VMMImpl->Protect(vmm, virtualAddress, count, protect & VMM_PAGE_PROTECT_BITS);
}

void VMMMap(void* vmm, void* virtualAddress, void* physicalAddress)
{
	if (!vmm)
		return;
	s_VMMImpl->Map(vmm, virtualAddress, physicalAddress);
}

void VMMMapLinear(void* vmm, void* virtualAddress, void* physicalAddress, size_t count)
{
	if (!vmm)
		return;
	s_VMMImpl->MapLinear(vmm, virtualAddress, physicalAddress, count);
}

void* VMMTranslate(void* vmm, void* virtualAddress)
{
	if (!vmm)
		return nullptr;
	return s_VMMImpl->Translate(vmm, virtualAddress);
}

void* VMMGetRootTable(void* vmm, uint8_t* levels, bool* use1GiB)
{
	if (!vmm)
		return nullptr;
	return s_VMMImpl->GetRootTable(vmm, levels, use1GiB);
}