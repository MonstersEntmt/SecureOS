#pragma once

#include <stdint.h>

void     HandleACPITables(void* rsdpAddress);
void*    GetLAPICAddress(void);
void*    GetIOAPICAddress(void);
uint8_t* GetLAPICIDs(uint8_t* lapicCount);