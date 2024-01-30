#pragma once

#include <stdint.h>

bool IDTClearVector(uint16_t vector);
bool IDTSetCallGate(uint16_t vector, void* target, uint16_t selector, uint8_t privilege);
bool IDTSetInterruptGate(uint16_t vector, void* target, uint16_t selector, uint8_t privilege, uint8_t ist);
bool IDTSetTrapGate(uint16_t vector, void* target, uint16_t selector, uint8_t privilege, uint8_t ist);

void  IDTLoad(void);
void* IDTGet(void);