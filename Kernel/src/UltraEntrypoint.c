#include "Build.h"
#include "IO/Debug.h"
#include "Panic.h"
#include "Ultra/UltraProtocol.h"

#include <stdint.h>
#include <stdio.h>

static void testFunc(const char* restrict fmt, ...)
{
	va_list vlist, vlist2;
	va_start(vlist, fmt);
	va_copy(vlist2, vlist);
	printf("testFunc: %Ls\n%Ls\n", fmt, &vlist, fmt, &vlist2);
	va_end(vlist);
	va_end(vlist2);
}

void kernel_entry(struct ultra_boot_context* bootContext, uint32_t magic)
{
#if BUILD_IS_CONFIG_DEBUG
	DebugSetupRedirects();
#endif
	if (!bootContext)
	{
		puts("kernel_entry CRITICAL: Ultra BootContext is nullptr");
		KernelPanic();
	}
	if (magic != ULTRA_MAGIC)
	{
		printf("kernel_entry CRITICAL: Ultra Magic %08X is not %08X\n", magic, ULTRA_MAGIC);
		KernelPanic();
	}
	printf("kernel_entry INFO: Ultra BootContext %p, Magic %08X\n", bootContext, magic);

	testFunc("Cool stuff %b, %u, %s", true, 15, "Nice shit indeed");

	KernelPanic();
}