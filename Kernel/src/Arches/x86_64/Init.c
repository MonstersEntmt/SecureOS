#include "Arches/x86_64/Features.h"
#include "Arches/x86_64/GDT.h"
#include "Arches/x86_64/IDT.h"
#include "Arches/x86_64/InterruptHandlers.h"
#include "Entry.h"
#include "Panic.h"
#include <stdio.h>

void KernelArchPreInit(void)
{
	puts("INFO: x86-64 pre init started");
	CPUDisableInterrupts();

	GDTSetNullDescriptor(0);
	GDTSetCodeDescriptor(8, 0, false);
	GDTSetDataDescriptor(16);
	GDTSetCodeDescriptor(24, 3, false);
	GDTSetDataDescriptor(32);

	IDTSetInterruptGate(0x00, &DEInterruptHandlerWrapper, 8, 0, 0);
	IDTSetInterruptGate(0x06, &UDInterruptHandlerWrapper, 8, 0, 0);
	IDTSetTrapGate(0x08, &DFExceptionHandlerWrapper, 8, 0, 0);
	IDTSetTrapGate(0x0D, &GPExceptionHandlerWrapper, 8, 0, 0);
	IDTSetTrapGate(0x0E, &PFExceptionHandlerWrapper, 8, 0, 0);

	GDTLoad(8, 16);
	LDTLoad(0);
	IDTLoad();
	
	CPUEnableInterrupts();

	KernelArchDetectAndEnableFeatures();
	puts("INFO: x86-64 pre init done");
}

void KernelArchPostInit(void)
{
	puts("INFO: x86-64 post init started");
	puts("INFO: x86-64 post init done");
}