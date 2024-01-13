#include "x86_64/KernelPageTable.h"
#include "PMM.h"

#include <string.h>

struct KPTState
{
	bool      IsLvl5;
	uint8_t   Pad[7];
	uint64_t* BaseTable;
};

struct KPTState g_KPT;

extern void x86_64KPTSetCR3(uint64_t cr3, bool isLvl5);

void x86_64KPTInit()
{
	struct PMMMemoryStats memoryStats;
	PMMGetMemoryStats(&memoryStats);

	g_KPT.IsLvl5    = false; // TODO(MarcasRealAccount): Determine if cpu supports level 5
	g_KPT.BaseTable = (uint64_t*) PMMAlloc();

	uint16_t nextPML5e = 0;
	uint16_t nextPML4e = 0;
	uint16_t nextPDPe  = 0;
	uint16_t nextPDe   = 0;

	uint64_t* curPML5;
	uint64_t* curPML4;
	if (g_KPT.IsLvl5)
	{
		curPML5 = g_KPT.BaseTable;
		curPML4 = (uint64_t*) PMMAlloc();
		memset(curPML5, 0, 4096);
		memset(curPML4, 0, 4096);
		curPML5[nextPML5e++] = 0x3 | ((uint64_t) curPML4 & 0x000F'FFFF'FFFF'F000UL);
	}
	else
	{
		curPML5 = nullptr;
		curPML4 = g_KPT.BaseTable;
		memset(curPML4, 0, 4096);
	}
	uint64_t* curPDP = (uint64_t*) PMMAlloc();
	uint64_t* curPD  = (uint64_t*) PMMAlloc();
	memset(curPDP, 0, 4096);
	memset(curPD, 0, 4096);
	curPML4[nextPML4e++] = 0x3 | ((uint64_t) curPDP & 0x000F'FFFF'FFFF'F000UL);
	curPDP[nextPDPe++]   = 0x3 | ((uint64_t) curPD & 0x000F'FFFF'FFFF'F000UL);

	uint64_t* lowerPT    = (uint64_t*) PMMAlloc();
	size_t    last4KPage = memoryStats.LastAddress > 0x10'0000 ? 512 : memoryStats.LastAddress / 4096;
	lowerPT[0]           = 0;
	for (size_t i = 1; i < last4KPage; ++i)
		lowerPT[i] = 0x3 | (i << 12);
	memset(lowerPT + last4KPage * 8, 0, (512 - last4KPage) * 8);

	curPD[nextPDe++]    = 0x3 | ((uint64_t) lowerPT & 0x000F'FFFF'FFFF'F000UL);
	uint64_t last2MPage = (memoryStats.LastAddress + 0xF'FFFF) / 0x10'0000;
	for (size_t i = 1; i < last2MPage; ++i)
	{
		curPD[nextPDe++] = 0x83 | (i << 21);
		if (nextPDe < 512)
			continue;

		nextPDe = 0;
		curPD   = (uint64_t*) PMMAlloc();
		memset(curPD, 0, 4096);
		curPDP[nextPDPe++] = 0x3 | ((uint64_t) curPD & 0x000F'FFFF'FFFF'F000UL);
		if (nextPDPe < 512)
			continue;

		if (!g_KPT.IsLvl5 && nextPML4e == 512)
		{
			// TODO(MarcasRealAccount): PANIC!!!
			break;
		}

		nextPDPe = 0;
		curPDP   = (uint64_t*) PMMAlloc();
		memset(curPDP, 0, 4096);
		curPML4[nextPML4e++] = 0x3 | ((uint64_t) curPDP & 0x000F'FFFF'FFFF'F000UL);
		if (!g_KPT.IsLvl5 || nextPML4e < 512)
			continue;

		if (nextPML5e == 512)
		{
			// TODO(MarcasRealAccount): PANIC!!!
			break;
		}

		nextPML4e = 0;
		curPML4   = (uint64_t*) PMMAlloc();
		memset(curPML4, 0, 4096);
		curPML5[nextPML5e++] = 0x3 | ((uint64_t) curPML4 & 0x000F'FFFF'FFFF'F000UL);
	}

	x86_64KPTSetCR3((uint64_t) g_KPT.BaseTable & 0x000F'FFFF'FFFF'F000UL, g_KPT.IsLvl5);
}