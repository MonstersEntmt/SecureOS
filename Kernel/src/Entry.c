#include "Build.h"
#include "DebugCon.h"
#include "Halt.h"
#include "PMM.h"
#include "Ultra/UltraProtocol.h"

#if BUILD_IS_ARCH_X86_64
	#include "x86_64/GDT.h"
	#include "x86_64/IDT.h"
	#include "x86_64/InterruptHandlers.h"
	#include "x86_64/KernelPageTable.h"
#endif

static void VisitUltraInvalidAttribute(void* userdata);
static void VisitUltraPlatformInfoAttribute(struct ultra_platform_info_attribute* attribute, void* userdata);
static void VisitUltraKernelInfoAttribute(struct ultra_kernel_info_attribute* attribute, void* userdata);
static void VisitUltraMemoryMapAttribute(struct ultra_memory_map_attribute* attribute, void* userdata);
static void VisitUltraModuleInfoAttribute(struct ultra_module_info_attribute* attribute, void* userdata);
static void VisitUltraCommandLineAttribute(struct ultra_command_line_attribute* attribute, void* userdata);
static void VisitUltraFramebufferAttribute(struct ultra_framebuffer_attribute* attribute, void* userdata);
static void VisitUltraAttribute(struct ultra_attribute_header* attribute, void* userdata);

void kernel_entry(struct ultra_boot_context* bootContext, uint32_t magic)
{
	if (magic != ULTRA_MAGIC)
	{
		DebugCon_WriteFormatted("Entry magic %08X is not %08X\r\n", magic, ULTRA_MAGIC);
		return;
	}

#if BUILD_IS_ARCH_X86_64
	DisableInterrupts();
	x86_64GDTClearDescriptors();
	x86_64GDTSetNullDescriptor(0);
	x86_64GDTSetCodeDescriptor(1, 0, false);
	x86_64GDTSetDataDescriptor(2);
	x86_64GDTSetCodeDescriptor(3, 3, false);
	x86_64GDTSetDataDescriptor(4);
	x86_64IDTClearDescriptors();
	x86_64IDTSetTrapGate(0x0D, (uint64_t) x86_64GPExceptionHandlerWrapper, 8, 0, 0);
	x86_64IDTSetInterruptGate(0x40, (uint64_t) x86_64TestInterruptHandlerWrapper, 8, 0, 0);
	x86_64LoadGDT(8, 16);
	x86_64LoadLDT(0);
	x86_64LoadIDT();
	EnableInterrupts();
#endif

	{
		struct ultra_attribute_header* curAttribute = bootContext->attributes;
		for (uint32_t i = 0; i < bootContext->attribute_count; ++i)
		{
			VisitUltraAttribute(curAttribute, nullptr);
			curAttribute = ULTRA_NEXT_ATTRIBUTE(curAttribute);
		}
	}

#if BUILD_IS_ARCH_X86_64
	x86_64KPTInit();
#endif
	PMMReclaim();
	PMMPrintMemoryMap();

	void* singlePage = PMMAlloc();
	DebugCon_WriteFormatted("Single Page: 0x%016X\r\n", singlePage);
	void* multiplePages = PMMAllocContiguous(16);
	DebugCon_WriteFormatted("Multiple Pages: 0x%016X\r\n", multiplePages);
	PMMFree(singlePage);
	PMMFreeContiguous(multiplePages, 16);

	CPUHalt();
}

void VisitUltraAttribute(struct ultra_attribute_header* attribute, void* userdata)
{
	switch (attribute->type)
	{
	case ULTRA_ATTRIBUTE_INVALID:
		VisitUltraInvalidAttribute(userdata);
		break;
	case ULTRA_ATTRIBUTE_PLATFORM_INFO:
		VisitUltraPlatformInfoAttribute((struct ultra_platform_info_attribute*) attribute, userdata);
		break;
	case ULTRA_ATTRIBUTE_KERNEL_INFO:
		VisitUltraKernelInfoAttribute((struct ultra_kernel_info_attribute*) attribute, userdata);
		break;
	case ULTRA_ATTRIBUTE_MEMORY_MAP:
		VisitUltraMemoryMapAttribute((struct ultra_memory_map_attribute*) attribute, userdata);
		break;
	case ULTRA_ATTRIBUTE_MODULE_INFO:
		VisitUltraModuleInfoAttribute((struct ultra_module_info_attribute*) attribute, userdata);
		break;
	case ULTRA_ATTRIBUTE_COMMAND_LINE:
		VisitUltraCommandLineAttribute((struct ultra_command_line_attribute*) attribute, userdata);
		break;
	case ULTRA_ATTRIBUTE_FRAMEBUFFER_INFO:
		VisitUltraFramebufferAttribute((struct ultra_framebuffer_attribute*) attribute, userdata);
		break;
	}
}

void VisitUltraInvalidAttribute(void* userdata)
{
	DebugCon_WriteString("ERROR: Invalid Ultra attribute encountered\r\n");
}

void VisitUltraPlatformInfoAttribute(struct ultra_platform_info_attribute* attribute, void* userdata)
{
	const char* platformType;
	switch (attribute->platform_type)
	{
	case ULTRA_PLATFORM_INVALID: platformType = "Invalid"; break;
	case ULTRA_PLATFORM_BIOS: platformType = "Bios"; break;
	case ULTRA_PLATFORM_UEFI: platformType = "UEFI"; break;
	default: platformType = "Unknown"; break;
	}

	DebugCon_WriteFormatted("Loaded by %s v%hu.%hu through %s\r\n",
							attribute->loader_name,
							attribute->loader_major,
							attribute->loader_minor,
							platformType);
}

void VisitUltraKernelInfoAttribute(struct ultra_kernel_info_attribute* attribute, void* userdata)
{
	DebugCon_WriteFormatted("Kernel loaded at %016lx(%016lx) -> %016lx(%lu)\r\n",
							attribute->virtual_base,
							attribute->physical_base,
							attribute->virtual_base + attribute->size,
							attribute->size);
}

static bool PMMUltraMemoryMapGetter(void* userdata, size_t index, struct PMMMemoryMapEntry* entry)
{
	struct ultra_memory_map_attribute* attribute  = (struct ultra_memory_map_attribute*) userdata;
	size_t                             entryCount = ULTRA_MEMORY_MAP_ENTRY_COUNT(attribute->header);
	if (index >= entryCount)
		return false;
	struct ultra_memory_map_entry* mapentry = attribute->entries + index;
	entry->Start                            = mapentry->physical_address;
	entry->Size                             = mapentry->size;
	switch (mapentry->type)
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

void VisitUltraMemoryMapAttribute(struct ultra_memory_map_attribute* attribute, void* userdata)
{
	size_t entryCount = ULTRA_MEMORY_MAP_ENTRY_COUNT(attribute->header);

	// DebugCon_WriteString("Memory Map:\r\n");
	// for (size_t i = 0; i < entryCount; ++i)
	// {
	// 	struct ultra_memory_map_entry* entry = attribute->entries + i;
	// 	const char*                    memoryType;
	// 	switch (entry->type)
	// 	{
	// 	case ULTRA_MEMORY_TYPE_INVALID: memoryType = "Invalid:"; break;
	// 	case ULTRA_MEMORY_TYPE_FREE: memoryType = "Free:"; break;
	// 	case ULTRA_MEMORY_TYPE_RESERVED: memoryType = "Reserved:"; break;
	// 	case ULTRA_MEMORY_TYPE_RECLAIMABLE: memoryType = "Reclaimable:"; break;
	// 	case ULTRA_MEMORY_TYPE_NVS: memoryType = "NVS:"; break;
	// 	case ULTRA_MEMORY_TYPE_LOADER_RECLAIMABLE: memoryType = "Loader Reclaimable:"; break;
	// 	case ULTRA_MEMORY_TYPE_MODULE: memoryType = "Module:"; break;
	// 	case ULTRA_MEMORY_TYPE_KERNEL_STACK: memoryType = "Kernel Stack:"; break;
	// 	case ULTRA_MEMORY_TYPE_KERNEL_BINARY: memoryType = "Kernel Binary:"; break;
	// 	default: memoryType = "Unknown:"; break;
	// 	}
	// 	DebugCon_WriteFormatted("  %-19s %016lx -> %016lx(%lu)\r\n",
	// 							memoryType,
	// 							entry->physical_address,
	// 							entry->physical_address + entry->size,
	// 							entry->size);
	// }

	PMMInit(entryCount, PMMUltraMemoryMapGetter, attribute);
}

void VisitUltraModuleInfoAttribute(struct ultra_module_info_attribute* attribute, void* userdata)
{
	const char* moduleType;
	switch (attribute->type)
	{
	case ULTRA_MODULE_TYPE_INVALID: moduleType = "Invalid"; break;
	case ULTRA_MODULE_TYPE_FILE: moduleType = "File"; break;
	case ULTRA_MODULE_TYPE_MEMORY: moduleType = "Memory"; break;
	default: moduleType = "Unknown"; break;
	}
	DebugCon_WriteFormatted("%s Module %s %016lx -> %016lx(%lu)\r\n",
							moduleType,
							attribute->name,
							attribute->address,
							attribute->address + attribute->size,
							attribute->size);
}

void VisitUltraCommandLineAttribute(struct ultra_command_line_attribute* attribute, void* userdata)
{
	DebugCon_WriteFormatted("Command line: %s\r\n", attribute->text);
}

void VisitUltraFramebufferAttribute(struct ultra_framebuffer_attribute* attribute, void* userdata)
{
	const char* formatType;
	switch (attribute->fb.format)
	{
	case ULTRA_FB_FORMAT_INVALID: formatType = "Invalid"; break;
	case ULTRA_FB_FORMAT_RGB888: formatType = "RGB"; break;
	case ULTRA_FB_FORMAT_BGR888: formatType = "BGR"; break;
	case ULTRA_FB_FORMAT_RGBX8888: formatType = "RGBA"; break;
	case ULTRA_FB_FORMAT_XRGB8888: formatType = "ARGB"; break;
	default: formatType = "Unknown"; break;
	}
	DebugCon_WriteFormatted("Framebuffer %ux%u(%u) %hubpp %s %016lx\r\n",
							attribute->fb.width,
							attribute->fb.height,
							attribute->fb.pitch,
							attribute->fb.bpp,
							formatType,
							attribute->fb.physical_address);
}