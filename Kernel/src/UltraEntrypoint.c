#include "Build.h"
#include "Init.h"
#include "IO/Debug.h"
#include "Panic.h"
#include "Ultra/UltraProtocol.h"

#include <stdint.h>
#include <stdio.h>

void kernel_entry(struct ultra_boot_context* bootContext, uint32_t magic)
{
#if BUILD_IS_CONFIG_DEBUG
	DebugSetupRedirects();
#endif
	if (!bootContext)
	{
		puts("CRITICAL: Ultra BootContext is nullptr");
		KernelPanic();
	}
	if (magic != ULTRA_MAGIC)
	{
		printf("CRITICAL: Ultra Magic %08X is not %08X\n", magic, ULTRA_MAGIC);
		KernelPanic();
	}
	printf("INFO: Ultra BootContext %p, Magic %08X\n", bootContext, magic);

	KernelArchPreInit();

	uint64_t hehe = *(uint64_t*) (0x6767'6767'6767'6767UL);
	printf("INFO: wait wut %016lX\n", hehe);

	KernelArchPostInit();

	KernelPanic();
}