#include "Ultra/UltraEntry.h"
#include "ACPI/ACPI.h"
#include "Build.h"
#include "Entry.h"
#include "IO/Debug.h"
#include "Memory/PMM.h"
#include "Panic.h"
#include "Ultra/UltraProtocol.h"
#include <stdio.h>

static const char s_PMMStatsMsg[] = "INFO: %s PMM Statistics\n"
									"      Address               %p\n"
									"      Footprint             %zu\n"
									"      Last Usable Address   %p\n"
									"      Last Physical Address %p\n"
									"      Pages Taken           %zu\n"
									"      Pages Free            %zu\n"
									"      Alloc Calls           %zu\n"
									"      Free Calls            %zu\n";

void UltraEntry(struct ultra_boot_context* bootContext, uint32_t magic)
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
	printf("INFO: Ultra BootContext v%hhu.%hhu\n", bootContext->protocol_major, bootContext->protocol_minor);

	KernelArchPreInit();

	UltraHandleAttribs(bootContext->attributes, bootContext->attribute_count);

	{
		struct PMMMemoryStats stats;
		PMMGetMemoryStats(&stats);
		printf(s_PMMStatsMsg,
			   PMMGetSelectedName(),
			   stats.Address,
			   stats.Footprint,
			   stats.LastUsableAddress,
			   stats.LastPhysicalAddress,
			   stats.PagesTaken,
			   stats.PagesFree,
			   stats.AllocCalls,
			   stats.FreeCalls);
	}

	KernelArchPostInit();

	KernelEntry();
}

static const char s_UltraAttributeError[] = "CRITICAL: Ultra attribute(s) missing\n"
											"          Platform Info %p\n"
											"          Memory Map    %p\n"
											"          Command Line  %p\n"
											"          Framebuffer   %p\n";

static bool UltraProtocolMemoryMapConverter(void* userdata, size_t index, struct PMMMemoryMapEntry* entry)
{
	struct ultra_memory_map_attribute* memoryMap = (struct ultra_memory_map_attribute*) userdata;
	if (index >= (ULTRA_MEMORY_MAP_ENTRY_COUNT(memoryMap->header)))
		return false;
	struct ultra_memory_map_entry* mapEntry = &memoryMap->entries[index];
	entry->Start                            = (uintptr_t) mapEntry->physical_address;
	entry->Size                             = (size_t) mapEntry->size;
	switch (mapEntry->type)
	{
	case ULTRA_MEMORY_TYPE_INVALID: entry->Type = PMMMemoryMapTypeInvalid; break;
	case ULTRA_MEMORY_TYPE_FREE: entry->Type = PMMMemoryMapTypeUsable; break;
	case ULTRA_MEMORY_TYPE_RESERVED: entry->Type = PMMMemoryMapTypeReserved; break;
	case ULTRA_MEMORY_TYPE_RECLAIMABLE: entry->Type = PMMMemoryMapTypeReclaimable; break;
	case ULTRA_MEMORY_TYPE_NVS: entry->Type = PMMMemoryMapTypeNVS; break;
	case ULTRA_MEMORY_TYPE_LOADER_RECLAIMABLE: entry->Type = PMMMemoryMapTypeLoaderReclaimable; break;
	case ULTRA_MEMORY_TYPE_MODULE: entry->Type = PMMMemoryMapTypeModule; break;
	case ULTRA_MEMORY_TYPE_KERNEL_STACK: entry->Type = PMMMemoryMapTypeKernel; break;
	case ULTRA_MEMORY_TYPE_KERNEL_BINARY: entry->Type = PMMMemoryMapTypeKernel; break;
	default: entry->Type = PMMMemoryMapTypeReserved; break;
	}
	return true;
}

void UltraHandleAttribs(struct ultra_attribute_header* attributes, uint32_t attributeCount)
{
	struct ultra_platform_info_attribute* platformInfo = nullptr;
	struct ultra_memory_map_attribute*    memoryMap    = nullptr;
	struct ultra_command_line_attribute*  commandLine  = nullptr;
	struct ultra_framebuffer_attribute*   framebuffer  = nullptr;

	struct ultra_attribute_header* curAttribute = attributes;
	for (uint32_t i = 0; i < attributeCount; ++i)
	{
		switch (curAttribute->type)
		{
		case ULTRA_ATTRIBUTE_PLATFORM_INFO:
			if (!platformInfo)
				platformInfo = (struct ultra_platform_info_attribute*) curAttribute;
			break;
		case ULTRA_ATTRIBUTE_MEMORY_MAP:
			if (!memoryMap)
				memoryMap = (struct ultra_memory_map_attribute*) curAttribute;
			break;
		case ULTRA_ATTRIBUTE_COMMAND_LINE:
			if (!commandLine)
				commandLine = (struct ultra_command_line_attribute*) curAttribute;
			break;
		case ULTRA_ATTRIBUTE_FRAMEBUFFER_INFO:
			if (!framebuffer)
				framebuffer = (struct ultra_framebuffer_attribute*) curAttribute;
			break;
		}
		curAttribute = ULTRA_NEXT_ATTRIBUTE(curAttribute);
	}
	if (!platformInfo ||
		!memoryMap ||
		!framebuffer ||
		!commandLine)
	{
		printf(s_UltraAttributeError, platformInfo, memoryMap, commandLine, framebuffer);
		KernelPanic();
	}

	KernelSelectAllocatorsFromCommandLine(commandLine->text);
	PMMInit(ULTRA_MEMORY_MAP_ENTRY_COUNT(memoryMap->header), UltraProtocolMemoryMapConverter, memoryMap);

	ACPISetRSDPAddress((void*) (uintptr_t) platformInfo->acpi_rsdp_address);
}