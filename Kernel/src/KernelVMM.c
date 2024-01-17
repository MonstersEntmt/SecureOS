#include "KernelVMM.h"
#include "PMM.h"
#include "VMM.h"

void* g_KVMM;

void KernelVMMInit()
{
	g_KVMM = VMMNewPageTable();
	if (!g_KVMM)
	{
		// TODO(MarcasRealAccount): PANIC
		return;
	}

	struct PMMMemoryStats memoryStats;
	PMMGetMemoryStats(&memoryStats);
	uint64_t lastAddress = memoryStats.LastAddress;

	uint64_t last4KPage        = lastAddress > 0x20'0000 ? 512 : lastAddress / 4096;
	uint64_t last2MPage        = (lastAddress + 0x1F'FFFF) / 0x20'0000;
	void*    identityBegin4KiB = VMMAllocAt(g_KVMM, 0x1000, last4KPage - 1, VMM_PAGE_TYPE_4KIB, VMM_PAGE_PROTECT_READ_WRITE_EXECUTE);
	void*    identityBegin2MiB = VMMAllocAt(g_KVMM, last4KPage * 4096, (last2MPage - 1) * 512, VMM_PAGE_TYPE_2MIB, VMM_PAGE_PROTECT_READ_WRITE_EXECUTE);
	VMMMapLinear(g_KVMM, identityBegin4KiB, (void*) 0x1000, last4KPage - 1);
	VMMMapLinear(g_KVMM, identityBegin2MiB, (void*) 0x20'0000, (last2MPage - 1) * 512);

	VMMActivate(g_KVMM);
}

void* GetKernelPageTable()
{
	return g_KVMM;
}