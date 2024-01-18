#include "ACPI/ACPI.h"
#include "Build.h"
#include "DebugCon.h"
#include "Halt.h"
#include "KernelVMM.h"
#include "PMM.h"
#include "Ultra/UltraProtocol.h"
#include "VMM.h"

#if BUILD_IS_ARCH_X86_64
	#include "x86_64/GDT.h"
	#include "x86_64/IDT.h"
	#include "x86_64/InterruptHandlers.h"
#endif

struct KernelStartupData
{
	void* RsdpAddress;
};

static void VisitUltraInvalidAttribute(struct KernelStartupData* userdata);
static void VisitUltraPlatformInfoAttribute(struct ultra_platform_info_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraKernelInfoAttribute(struct ultra_kernel_info_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraMemoryMapAttribute(struct ultra_memory_map_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraModuleInfoAttribute(struct ultra_module_info_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraCommandLineAttribute(struct ultra_command_line_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraFramebufferAttribute(struct ultra_framebuffer_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraAttribute(struct ultra_attribute_header* attribute, struct KernelStartupData* userdata);

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

	struct KernelStartupData kernelStartupData;

	{
		struct ultra_attribute_header* curAttribute = bootContext->attributes;
		for (uint32_t i = 0; i < bootContext->attribute_count; ++i)
		{
			VisitUltraAttribute(curAttribute, &kernelStartupData);
			curAttribute = ULTRA_NEXT_ATTRIBUTE(curAttribute);
		}
	}

	KernelVMMInit();
	HandleACPITables(kernelStartupData.RsdpAddress);
	PMMReclaim();

	{
		struct PMMMemoryStats memoryStats;
		PMMGetMemoryStats(&memoryStats);
		DebugCon_WriteFormatted("PMM Stats:\n  Footprint: %lu\n  Last Usable Address: 0x%016lu\n  Last Address: 0x%016lu\n  Pages Free: %lu\n", (memoryStats.AllocatorFootprint + 4095) / 4096, memoryStats.LastUsableAddress, memoryStats.LastAddress, memoryStats.PagesFree);
	}

	{
		struct VMMMemoryStats memoryStats;
		VMMGetMemoryStats(GetKernelPageTable(), &memoryStats);
		DebugCon_WriteFormatted("Kernel VMM Stats:\n  Footprint: %lu\n  Pages Allocated: %lu\n", (memoryStats.AllocatorFootprint + 4095) / 4096, memoryStats.PagesAllocated);
	}

	CPUHalt();
}

void VisitUltraAttribute(struct ultra_attribute_header* attribute, struct KernelStartupData* userdata)
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

void VisitUltraInvalidAttribute(struct KernelStartupData* userdata)
{
	DebugCon_WriteString("ERROR: Invalid Ultra attribute encountered\r\n");
}

void VisitUltraPlatformInfoAttribute(struct ultra_platform_info_attribute* attribute, struct KernelStartupData* userdata)
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

	userdata->RsdpAddress = (void*) attribute->acpi_rsdp_address;
}

void VisitUltraKernelInfoAttribute(struct ultra_kernel_info_attribute* attribute, struct KernelStartupData* userdata)
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
	case ULTRA_MEMORY_TYPE_RECLAIMABLE: entry->Type = PMMMemoryMapTypeACPI; break; // INFO(MarcasRealAccount): As UEFI and Hyper only gives a Reclaimable region for ACPI tables, we always assume as such
	case ULTRA_MEMORY_TYPE_NVS: entry->Type = PMMMemoryMapTypeNVS; break;
	case ULTRA_MEMORY_TYPE_LOADER_RECLAIMABLE: entry->Type = PMMMemoryMapTypeLoaderReclaimable; break;
	case ULTRA_MEMORY_TYPE_MODULE: entry->Type = PMMMemoryMapTypeModule; break;
	case ULTRA_MEMORY_TYPE_KERNEL_STACK: entry->Type = PMMMemoryMapTypeKernel; break;
	case ULTRA_MEMORY_TYPE_KERNEL_BINARY: entry->Type = PMMMemoryMapTypeKernel; break;
	default: entry->Type = PMMMemoryMapTypeReserved; break;
	}
	return true;
}

void VisitUltraMemoryMapAttribute(struct ultra_memory_map_attribute* attribute, struct KernelStartupData* userdata)
{
	size_t entryCount = ULTRA_MEMORY_MAP_ENTRY_COUNT(attribute->header);
	PMMInit(entryCount, PMMUltraMemoryMapGetter, attribute);
}

void VisitUltraModuleInfoAttribute(struct ultra_module_info_attribute* attribute, struct KernelStartupData* userdata)
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

void VisitUltraCommandLineAttribute(struct ultra_command_line_attribute* attribute, struct KernelStartupData* userdata)
{
	DebugCon_WriteFormatted("Command line: %s\r\n", attribute->text);
}

void VisitUltraFramebufferAttribute(struct ultra_framebuffer_attribute* attribute, struct KernelStartupData* userdata)
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