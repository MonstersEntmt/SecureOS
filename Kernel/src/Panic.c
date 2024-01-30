#include "Panic.h"

void KernelPanic(void)
{
	// TODO(MarcasRealAccount): SMPHaltCores()
	CPUHalt();
}