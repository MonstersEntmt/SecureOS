#include "x86_64/InterruptHandlers.h"
#include "DebugCon.h"

void x86_64GPExceptionHandler(const struct x86_64InterruptState* state, uint16_t code)
{
	DebugCon_WriteFormatted("GP(0x%04X):\n  RIP = 0x%016llX\n  RSP = 0x%016llX\n  RFLAGS = 0x%08X\n  CS = 0x%04hX\n  SS = 0x%04hX\n", (uint32_t) code, state->rip, state->rsp, (uint32_t) state->rflags, state->cs, state->ss);
}

void x86_64TestInterruptHandler(const struct x86_64InterruptState* state)
{
	DebugCon_WriteFormatted("Test Interrupt called with state:\n  RIP = 0x%016llX\n  RSP = 0x%016llX\n  RFLAGS = 0x%08X\n  CS = 0x%04hX\n  SS = 0x%04hX\n", state->rip, state->rsp, (uint32_t) state->rflags, state->cs, state->ss);
}