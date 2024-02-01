#include "Arches/x86_64/Features.h"
#include <stdio.h>

void KernelArchDetectAndEnableFeatures(void)
{
	puts("INFO: Enabling NXE");
	ArchEnableNXE();
}