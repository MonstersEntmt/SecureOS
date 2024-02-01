#include "Entry.h"
#include "Memory/PMM.h"
#include "Memory/VMM.h"
#include "Panic.h"

#include <stdio.h>
#include <string.h>

void* g_KernelVMM = nullptr;

void KernelSelectAllocatorsFromCommandLine(const char* commandLine)
{
	size_t commandLineLen = strlen(commandLine);
	size_t commandBegin   = 0;
	while (commandBegin < commandLineLen)
	{
		size_t commandEnd = (const char*) memchr(commandLine + commandBegin, ' ', commandLineLen - commandBegin) - commandLine;
		size_t commandLen = commandEnd - commandBegin;
		if (commandLen > 5)
		{
			if (memcmp(commandLine + commandBegin, "-pmm=", 5) == 0)
				PMMSelect(commandLine + commandBegin + 5, commandLen - 5);
			else if (memcmp(commandLine + commandBegin, "-vmm=", 5) == 0)
				VMMSelect(commandLine + commandBegin + 5, commandLen - 5);
		}
		commandBegin = commandEnd + 1;
	}
}

void KernelSetupVMM(void)
{
	g_KernelVMM = VMMCreate();
	if (!g_KernelVMM)
	{
		puts("CRITICAL: Kernel failed to create VMM");
		KernelPanic();
	}

	struct PMMMemoryStats memoryStats;
	PMMGetMemoryStats(&memoryStats);
	uintptr_t lastAddress = (uintptr_t) memoryStats.LastPhysicalAddress;

	uint64_t last4KPage        = lastAddress > 0x20'0000 ? 512 : lastAddress >> 12;
	uint64_t last2MPage        = (lastAddress + 0x1F'FFFF) >> 21;
	void*    identityBegin4KiB = VMMAllocAt(g_KernelVMM, (void*) 0x1000, last4KPage - 1, VMM_PAGE_SIZE_4KiB | VMM_PAGE_PROTECT_READ_WRITE_EXECUTE);
	void*    identityBegin2MiB = VMMAllocAt(g_KernelVMM, (void*) (last4KPage << 12), (last2MPage - 1) << 9, VMM_PAGE_SIZE_2MiB | VMM_PAGE_PROTECT_READ_WRITE_EXECUTE);
	VMMMapLinear(g_KernelVMM, identityBegin4KiB, (void*) 0x1000, last4KPage - 1);
	VMMMapLinear(g_KernelVMM, identityBegin2MiB, (void*) 0x20'0000, (last2MPage - 1) << 9);

	VMMActivate(g_KernelVMM);
}

void* KernelGetVMM(void)
{
	return g_KernelVMM;
}

void KernelEntry(void)
{
	puts("INFO: Kernel entry reached");
	KernelPanic();
}