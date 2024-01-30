#pragma once

#include <stdint.h>

struct InterruptState
{
	uint64_t RIP;
	uint16_t CS;
	uint64_t RFlags;
	uint64_t RSP;
	uint16_t SS;
};

void DEInterruptHandler(const struct InterruptState* state);
void UDInterruptHandler(const struct InterruptState* state);
void DFExceptionHandler(const struct InterruptState* state, uint16_t code);
void GPExceptionHandler(const struct InterruptState* state, uint16_t code);
void PFExceptionHandler(const struct InterruptState* state, uint16_t code);

extern void DEInterruptHandlerWrapper(void);
extern void UDInterruptHandlerWrapper(void);
extern void DFExceptionHandlerWrapper(void);
extern void GPExceptionHandlerWrapper(void);
extern void PFExceptionHandlerWrapper(void);