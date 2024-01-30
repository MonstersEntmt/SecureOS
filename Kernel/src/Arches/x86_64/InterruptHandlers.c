#include "Arches/x86_64/InterruptHandlers.h"
#include "Arches/x86_64/Paging.h"
#include "Panic.h"
#include <stdio.h>

void DEInterruptHandler(const struct InterruptState* state)
{
	printf("DE: RIP=%p, CS=%04hX, RFlags=%08lX, RSP=%p, SS=%04hX\n", state->RIP, state->CS, state->RFlags, state->RSP, state->SS);
	KernelPanic();
}

void UDInterruptHandler(const struct InterruptState* state)
{
	printf("UD: RIP=%p, CS=%04hX, RFlags=%08lX, RSP=%p, SS=%04hX\n", state->RIP, state->CS, state->RFlags, state->RSP, state->SS);
	KernelPanic();
}

void DFExceptionHandler(const struct InterruptState* state, uint16_t code)
{
	printf("DF(%04hX): RIP=%p, CS=%04hX, RFlags=%08lX, RSP=%p, SS=%04hX\n", code, state->RIP, state->CS, state->RFlags, state->RSP, state->SS);
	KernelPanic();
}

void GPExceptionHandler(const struct InterruptState* state, uint16_t code)
{
	printf("GP(%04hX): RIP=%p, CS=%04hX, RFlags=%08lX, RSP=%p, SS=%04hX\n", code, state->RIP, state->CS, state->RFlags, state->RSP, state->SS);
	KernelPanic();
}

void PFExceptionHandler(const struct InterruptState* state, uint16_t code)
{
	printf("PF(%04hX): CR2=%p RIP=%p, CS=%04hX, RFlags=%08lX, RSP=%p, SS=%04hX\n", code, PagingGetCR2(), state->RIP, state->CS, state->RFlags, state->RSP, state->SS);
	KernelPanic();
}