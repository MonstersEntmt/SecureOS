#pragma once

#include <stdint.h>

bool GDTSetNullDescriptor(uint16_t descriptor);
bool GDTSetCodeDescriptor(uint16_t descriptor, uint8_t privilege, bool conforming);
bool GDTSetDataDescriptor(uint16_t descriptor);
bool GDTSetLDTDescriptor(uint16_t descriptor, uint8_t privilege, void* address, uint32_t limit);

void  GDTLoad(uint16_t initialCS, uint16_t initialDS);
void* GDTGet(void);
void  LDTLoad(uint16_t descriptor);
void* LDTGet(uint16_t descriptor);