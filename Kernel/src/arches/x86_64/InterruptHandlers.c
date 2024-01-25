#include "x86_64/InterruptHandlers.h"
#include "Log.h"

void x86_64GPExceptionHandler(const struct x86_64InterruptState* state, uint16_t code)
{
	LogErrorFormatted("GP",
					  "e: 0x%04hX, RIP: 0x%016lX, RSP: 0x%016lX, RFLAGS: 0x%08X, CS: 0x%04hX, SS: 0x%04hX",
					  code,
					  state->rip,
					  state->rsp,
					  (uint32_t) state->rflags,
					  state->cs,
					  state->ss);
}

void x86_64TestInterruptHandler(const struct x86_64InterruptState* state)
{
	LogErrorFormatted("TestInt",
					  "RIP: 0x%016lX, RSP: 0x%016lX, RFLAGS: 0x%08X, CS: 0x%04hX, SS: 0x%04hX",
					  state->rip,
					  state->rsp,
					  (uint32_t) state->rflags,
					  state->cs,
					  state->ss);
}