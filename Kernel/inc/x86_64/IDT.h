#pragma once

#include <stdint.h>

bool x86_64IDTClearDescriptors(void);
bool x86_64IDTSetCallGate(uint16_t vector, uint64_t target, uint16_t selector, uint8_t privilege);
bool x86_64IDTSetInterruptGate(uint16_t vector, uint64_t target, uint16_t selector, uint8_t privilege, uint8_t ist);
bool x86_64IDTSetTrapGate(uint16_t vector, uint64_t target, uint16_t selector, uint8_t privilege, uint8_t ist);

void x86_64LoadIDT(void);