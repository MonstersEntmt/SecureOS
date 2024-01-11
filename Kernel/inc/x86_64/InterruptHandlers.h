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

void        x86_64GPExceptionHandler(const struct x86_64InterruptState* state, uint16_t code);
extern void x86_64GPExceptionHandlerWrapper(void);

void        x86_64TestInterruptHandler(const struct x86_64InterruptState* state);
extern void x86_64TestInterruptHandlerWrapper(void);