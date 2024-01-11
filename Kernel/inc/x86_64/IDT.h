#pragma once

#include <stdint.h>

struct x86_64InterruptState
{
	uint64_t rip;
	uint16_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint16_t ss;
};

bool x86_64IDTClearDescriptors();
bool x86_64IDTSetCallGate(uint16_t vector, uint64_t target, uint16_t selector, uint8_t privilege);
bool x86_64IDTSetInterruptGate(uint16_t vector, uint64_t target, uint16_t selector, uint8_t privilege, uint8_t ist);
bool x86_64IDTSetTrapGate(uint16_t vector, uint64_t target, uint16_t selector, uint8_t privilege, uint8_t ist);

void x86_64LoadIDT(void);