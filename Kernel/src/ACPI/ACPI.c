#include "ACPI/ACPI.h"

void* g_RSDPAddress;

void ACPISetRSDPAddress(void* rsdpAddress)
{
	g_RSDPAddress = rsdpAddress;
}