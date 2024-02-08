#pragma once

#include <stddef.h>
#include <stdint.h>

struct CPUFeatures
{
	uint64_t FXSAVE : 1;
	uint64_t XSAVE  : 1;
	uint64_t SSE1   : 1;
	uint64_t SSE2   : 1;
	uint64_t SSE3   : 1;
	uint64_t SSSE3  : 1;
	uint64_t SSE4_1 : 1;
	uint64_t SSE4_2 : 1;
	uint64_t SSE4A  : 1;
	uint64_t AVX    : 1;
	uint64_t AVX2   : 1;
	uint64_t AVX512 : 1;
	uint64_t XOP    : 1;
	uint64_t FMA    : 1;
	uint64_t FMA4   : 1;
};

extern struct CPUFeatures g_Features;
extern size_t             g_XSaveSize;

void KernelArchDetectAndEnableFeatures(void);

void ArchEnableNXE(void);
void ArchEnableSSE(void);