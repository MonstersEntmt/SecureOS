#pragma once

#include <stdint.h>

extern struct x86_64TrampolineSettings
{
	uint64_t PageTable;
	uint64_t PageTableSettings;
	uint64_t CPUTrampolineFn;
	uint64_t StackAllocFn;
} g_x86_64TrampolineSettings;
extern struct x86_64TrampolineStats
{
	uint8_t Alive;
	uint8_t Ack;
} g_x86_64TrampolineStats;
extern void x86_64Trampoline(void);