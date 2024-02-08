#include "Arches/x86_64/Features.h"
#include <stdio.h>

struct CPUFeatures g_Features = (struct CPUFeatures) {
	.FXSAVE = false,
	.XSAVE  = false,
	.SSE1   = false,
	.SSE2   = false,
	.SSE3   = false,
	.SSSE3  = false,
	.SSE4_1 = false,
	.SSE4_2 = false,
	.SSE4A  = false,
	.AVX    = false,
	.AVX2   = false,
	.AVX512 = false,
	.XOP    = false,
	.FMA    = false,
	.FMA4   = false
};

size_t g_XSaveSize = 0;

static const char* c_FeaturesFMT = "INFO: Features enabled\n"
								   "      FXSAVE: %b\n"
								   "      XSAVE:  %b\n"
								   "      SSE1:   %b\n"
								   "      SSE2:   %b\n"
								   "      SSE3:   %b\n"
								   "      SSSE3:  %b\n"
								   "      SSE4.1: %b\n"
								   "      SSE4.2: %b\n"
								   "      SSE4A:  %b\n"
								   "      AVX:    %b\n"
								   "      AVX2:   %b\n"
								   "      AVX512: %b\n"
								   "      XOP:    %b\n"
								   "      FMA:    %b\n"
								   "      FMA4:   %b\n";

void KernelArchDetectAndEnableFeatures(void)
{
	puts("INFO: Enabling NXE");
	ArchEnableNXE();
	puts("INFO: Enabling SSE");
	ArchEnableSSE();
	printf(c_FeaturesFMT,
		   g_Features.FXSAVE,
		   g_Features.XSAVE,
		   g_Features.SSE1,
		   g_Features.SSE2,
		   g_Features.SSE3,
		   g_Features.SSSE3,
		   g_Features.SSE4_1,
		   g_Features.SSE4_2,
		   g_Features.SSE4A,
		   g_Features.AVX,
		   g_Features.AVX2,
		   g_Features.AVX512,
		   g_Features.XOP,
		   g_Features.FMA,
		   g_Features.FMA4);
}