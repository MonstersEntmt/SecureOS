#pragma once

#include <stdint.h>

bool x86_64GDTClearDescriptors(void);
bool x86_64GDTSetNullDescriptor(uint16_t descriptor);
bool x86_64GDTSetCodeDescriptor(uint16_t descriptor, uint8_t privilege, bool conforming);
bool x86_64GDTSetDataDescriptor(uint16_t descriptor);

void x86_64LoadGDT(void);
void x86_64LoadLDT(uint16_t segment);