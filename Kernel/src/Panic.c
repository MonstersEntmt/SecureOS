#include "Panic.h"
#include <stdio.h>

void KernelPanic(void)
{
	puts("PANIC");
	// TODO(MarcasRealAccount): SMPHaltCores()
	CPUHalt();
}