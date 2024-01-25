#include "ACPI/ACPI.h"
#include "Build.h"
#include "DebugCon.h"
#include "Graphics/Graphics.h"
#include "Halt.h"
#include "KernelVMM.h"
#include "Log.h"
#include "PMM.h"
#include "Ultra/UltraProtocol.h"
#include "VMM.h"

#include <string.h>

#if BUILD_IS_ARCH_X86_64
	#include "x86_64/GDT.h"
	#include "x86_64/IDT.h"
	#include "x86_64/InterruptHandlers.h"
	#include "x86_64/Trampoline.h"
	#include "x86_64/Features.h"
#endif

struct KernelStartupData
{
	void* RsdpAddress;

	struct Framebuffer Framebuffer;

	void* BasicLatin;
};

static bool UltraProtocolMemoryMapConverter(void* userdata, size_t index, struct PMMMemoryMapEntry* entry);
static void UltraProtocolPrintAttributes(struct ultra_attribute_header* firstAttribute, uint32_t attributeCount);

bool    g_LapicWaitLock = false;
uint8_t g_LapicsRunning = 0;
void    CPUTrampoline(uint8_t lapicID);
void*   CPUStackAlloc(void);

void kernel_entry(struct ultra_boot_context* bootContext, uint32_t magic)
{
	LogDebugFormatted("Entry", "BootContext: 0x%016lX, magic: %08X", (uint64_t) bootContext, magic);
	if (!bootContext)
	{
		LogCritical("Entry", "BootContext is nullptr");
		CPUHalt();
		return;
	}
	if (magic != ULTRA_MAGIC)
	{
		LogCriticalFormatted("Entry", "Magic %08X is not %08X", magic, ULTRA_MAGIC);
		CPUHalt();
		return;
	}

#if BUILD_IS_ARCH_X86_64
	DisableInterrupts();
	if (!x86_64FeatureEnable())
	{
		LogCritical("Entry", "Your CPU does not support required features, terminating");
		CPUHalt();
		return;
	}
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
	memset(&kernelStartupData, 0, sizeof(struct KernelStartupData));

	{
		struct ultra_attribute_header* curAttribute = bootContext->attributes;
		for (uint32_t i = 0; i < bootContext->attribute_count; ++i)
		{
			switch (curAttribute->type)
			{
			case ULTRA_ATTRIBUTE_PLATFORM_INFO:
			{
				struct ultra_platform_info_attribute* platformAttrib = (struct ultra_platform_info_attribute*) curAttribute;
				kernelStartupData.RsdpAddress                        = (void*) platformAttrib->acpi_rsdp_address;
				break;
			}
			case ULTRA_ATTRIBUTE_MODULE_INFO:
			{
				struct ultra_module_info_attribute* moduleAttrib = (struct ultra_module_info_attribute*) curAttribute;
				if (strcmp(moduleAttrib->name, "basic-latin.font") == 0)
				{
					kernelStartupData.BasicLatin = (void*) moduleAttrib->address;
					break;
				}
				break;
			}
			case ULTRA_ATTRIBUTE_MEMORY_MAP:
			{
				struct ultra_memory_map_attribute* memMapAttrib = (struct ultra_memory_map_attribute*) curAttribute;

				size_t entryCount = ULTRA_MEMORY_MAP_ENTRY_COUNT(*curAttribute);
				PMMInit(entryCount, UltraProtocolMemoryMapConverter, memMapAttrib);
				break;
			}
			case ULTRA_ATTRIBUTE_FRAMEBUFFER_INFO:
			{
				struct ultra_framebuffer_attribute* fbAttrib = (struct ultra_framebuffer_attribute*) curAttribute;
				kernelStartupData.Framebuffer.Width          = fbAttrib->fb.width;
				kernelStartupData.Framebuffer.Height         = fbAttrib->fb.height;
				kernelStartupData.Framebuffer.Pitch          = fbAttrib->fb.pitch;
				kernelStartupData.Framebuffer.Content        = (void*) fbAttrib->fb.physical_address;
				kernelStartupData.Framebuffer.Colorspace     = FramebufferColorspaceSRGB;
				switch (fbAttrib->fb.format)
				{
				case ULTRA_FB_FORMAT_RGB888: kernelStartupData.Framebuffer.Format = FramebufferFormatRGB8; break;
				case ULTRA_FB_FORMAT_BGR888: kernelStartupData.Framebuffer.Format = FramebufferFormatBGR8; break;
				case ULTRA_FB_FORMAT_RGBX8888: kernelStartupData.Framebuffer.Format = FramebufferFormatRGBA8; break;
				case ULTRA_FB_FORMAT_XRGB8888: kernelStartupData.Framebuffer.Format = FramebufferFormatARGB8; break;
				}
				break;
			}
			}
			curAttribute = ULTRA_NEXT_ATTRIBUTE(curAttribute);
		}
	}

	KernelVMMInit();
	LoadFont((struct FontHeader*) kernelStartupData.BasicLatin);
	LogInit(&kernelStartupData.Framebuffer);
	UltraProtocolPrintAttributes(bootContext->attributes, bootContext->attribute_count);
	HandleACPITables(kernelStartupData.RsdpAddress);
	PMMReclaim();

	{
		struct PMMMemoryStats memoryStats;
		PMMGetMemoryStats(&memoryStats);
		LogDebugFormatted("PMM", "Address:             0x%016lX", memoryStats.AllocatorAddress);
		LogDebugFormatted("PMM", "Footprint:           %lu", (memoryStats.AllocatorFootprint + 4095) / 4096);
		LogDebugFormatted("PMM", "Last Usable Address: 0x%016lX", memoryStats.LastUsableAddress);
		LogDebugFormatted("PMM", "Last Address:        0x%016lX", memoryStats.LastAddress);
		LogDebugFormatted("PMM", "Pages Taken:         0x%016lX", memoryStats.PagesTaken);
		LogDebugFormatted("PMM", "Pages Free:          0x%016lX", memoryStats.PagesFree);
	}

	{
		void*                 kernelPageTable = GetKernelPageTable();
		struct VMMMemoryStats memoryStats;
		VMMGetMemoryStats(kernelPageTable, &memoryStats);
		LogDebugFormatted("VMM", "Address:         0x%016lX", (uint64_t) kernelPageTable);
		LogDebugFormatted("VMM", "Footprint:       %lu", (memoryStats.AllocatorFootprint + 4095) / 4096);
		LogDebugFormatted("VMM", "Pages Allocated: %lu", memoryStats.PagesAllocated);
	}

	uint8_t  lapicCount = 0;
	uint8_t* lapicIDs   = GetLAPICIDs(&lapicCount);
	if (lapicCount > 1)
	{
		LogDebugFormatted("SMP", "Booting up %hhu additional cores", lapicCount - 1);
		void*   kernelPageTable        = GetKernelPageTable();
		uint8_t kernelPageTableLevels  = 0;
		bool    kernelPageTableUse1GiB = false;
		void*   kernelRootPage         = VMMGetRootTable(kernelPageTable, &kernelPageTableLevels, &kernelPageTableUse1GiB);
		void*   tempRootPageTable      = PMMAllocBelow(1, 0xFFFF'FFFFU);
		memcpy(tempRootPageTable, kernelRootPage, 4096);

#if BUILD_IS_ARCH_X86_64
		g_x86_64TrampolineSettings = (struct x86_64TrampolineSettings) {
			.PageTable         = (uint64_t) tempRootPageTable,
			.PageTableSettings = (kernelPageTableLevels == 5 ? 1 : 0) | (kernelPageTableUse1GiB ? 2 : 0),
			.CPUTrampolineFn   = (uint64_t) CPUTrampoline,
			.StackAllocFn      = (uint64_t) CPUStackAlloc
		};
		memcpy((void*) 0x1000, x86_64Trampoline, 4096);

		struct x86_64TrampolineStats* trampolineStats = (struct x86_64TrampolineStats*) (0x1000 + ((uint64_t) &g_x86_64TrampolineStats - (uint64_t) x86_64Trampoline));
#endif

		void*     lapicAddress   = GetLAPICAddress();
		uint32_t* lapicRegisters = (uint32_t*) lapicAddress;

		g_LapicWaitLock = true;
		for (size_t i = 1; i < lapicCount; ++i) // Hoping implementations uphold the ACPI specification with first lapicID being the boot core
		{
			uint8_t id = lapicIDs[i];

			uint8_t pLapicsRunning = g_LapicsRunning;

			*trampolineStats = (struct x86_64TrampolineStats) {
				.Alive = 0,
				.Ack   = 0
			};

			LogDebugFormatted("SMP", "Sending startup IPI to core %hhu", id);

			lapicRegisters[0xA0] = 0;
			lapicRegisters[0xC4] = lapicRegisters[0xC4] & 0x00FF'FFFF | (id << 24);
			lapicRegisters[0xC0] = lapicRegisters[0xC0] & 0xFFF0'0000 | 0xC500;
			while (lapicRegisters[0xC0] & 4096);
			lapicRegisters[0xC4] = lapicRegisters[0xC4] & 0x00FF'FFFF | (id << 24);
			lapicRegisters[0xC0] = lapicRegisters[0xC0] & 0xFFF0'0000 | 0x8500;
			while (lapicRegisters[0xC0] & 4096);

			for (uint8_t j = 0; j < 2; ++j)
			{
				lapicRegisters[0xA0] = 0;
				lapicRegisters[0xC4] = lapicRegisters[0xC4] & 0x00FF'FFFF | (id << 24);
				uint32_t val         = lapicRegisters[0xC0] & 0xFFF0'F800 | 0x0601;
				lapicRegisters[0xC0] = val;
				while (lapicRegisters[0xC0] & 4096);
			}

			while (!trampolineStats->Alive); // Wait for cpu to be alive
			LogDebugFormatted("SMP", "Core %hhu alive", id);
			trampolineStats->Ack = true;

			while (g_LapicsRunning == pLapicsRunning); // Synchronize with lapic bootstrap
			LogDebugFormatted("SMP", "Core %hhu booted", id);
		}
		g_LapicWaitLock = false;

		PMMFree(tempRootPageTable, 1);
	}

	CPUHalt();
}

void CPUTrampoline(uint8_t lapicID)
{
	x86_64LoadGDT(8, 16);
	x86_64LoadLDT(0);
	x86_64LoadIDT();
	EnableInterrupts();

	void* pageTable = GetKernelPageTable();
	VMMActivate(pageTable);

	++g_LapicsRunning;
	LogDebug("SMP", "Booted");
	while (g_LapicWaitLock);

	CPUHalt();
}

void* CPUStackAlloc(void)
{
	void* kernelPageTable = GetKernelPageTable();
	void* stack           = VMMAlloc(kernelPageTable, 4, 0, VMM_PAGE_TYPE_4KIB, VMM_PAGE_PROTECT_READ_WRITE);
	for (size_t i = 0; i < 4; ++i)
	{
		void* physical = PMMAlloc(1);
		if (!physical)
			return nullptr; // TODO(MarcasRealAccount): PANIC
		VMMMap(kernelPageTable, (uint8_t*) stack + i * 4096, physical);
	}
	return stack + 4 * 4096;
}

bool UltraProtocolMemoryMapConverter(void* userdata, size_t index, struct PMMMemoryMapEntry* entry)
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

void UltraProtocolPrintAttributes(struct ultra_attribute_header* curAttribute, uint32_t attributeCount)
{
#if BUILD_IS_CONFIG_DEBUG
	for (size_t i = 0; i < attributeCount; ++i)
	{
		switch (curAttribute->type)
		{
		case ULTRA_ATTRIBUTE_INVALID:
		{
			LogDebugFormatted("Ultra", "Invalid attribute %lu", i);
			break;
		}
		case ULTRA_ATTRIBUTE_PLATFORM_INFO:
		{
			struct ultra_platform_info_attribute* platformAttrib = (struct ultra_platform_info_attribute*) curAttribute;

			const char* platformType;
			switch (platformAttrib->platform_type)
			{
			case ULTRA_PLATFORM_INVALID: platformType = "Invalid"; break;
			case ULTRA_PLATFORM_BIOS: platformType = "Bios"; break;
			case ULTRA_PLATFORM_UEFI: platformType = "UEFI"; break;
			default: platformType = "Unknown"; break;
			}

			LogDebugFormatted("Ultra",
							  "Loaded by %s v%hu.%hu through %s",
							  platformAttrib->loader_name,
							  platformAttrib->loader_major,
							  platformAttrib->loader_minor,
							  platformType);
			break;
		}
		case ULTRA_ATTRIBUTE_KERNEL_INFO:
		{
			struct ultra_kernel_info_attribute* kernelAttrib = (struct ultra_kernel_info_attribute*) curAttribute;

			LogDebugFormatted("Ultra",
							  "Kernel loaded at %016lx(%016lx) -> %016lx(%lu)",
							  kernelAttrib->virtual_base,
							  kernelAttrib->physical_base,
							  kernelAttrib->virtual_base + kernelAttrib->size,
							  kernelAttrib->size);
			break;
		}
		case ULTRA_ATTRIBUTE_MEMORY_MAP:
		{
			break;
		}
		case ULTRA_ATTRIBUTE_MODULE_INFO:
		{
			struct ultra_module_info_attribute* moduleAttrib = (struct ultra_module_info_attribute*) curAttribute;

			const char* moduleType;
			switch (moduleAttrib->type)
			{
			case ULTRA_MODULE_TYPE_INVALID: moduleType = "Invalid"; break;
			case ULTRA_MODULE_TYPE_FILE: moduleType = "File"; break;
			case ULTRA_MODULE_TYPE_MEMORY: moduleType = "Memory"; break;
			default: moduleType = "Unknown"; break;
			}
			LogDebugFormatted("Ultra",
							  "%s Module %s %016lx -> %016lx(%lu)",
							  moduleType,
							  moduleAttrib->name,
							  moduleAttrib->address,
							  moduleAttrib->address + moduleAttrib->size,
							  moduleAttrib->size);
			break;
		}
		case ULTRA_ATTRIBUTE_COMMAND_LINE:
		{
			struct ultra_command_line_attribute* cmdAttrib = (struct ultra_command_line_attribute*) curAttribute;
			LogDebugFormatted("Ultra", "Command line: %s", cmdAttrib->text);
			break;
		}
		case ULTRA_ATTRIBUTE_FRAMEBUFFER_INFO:
		{
			struct ultra_framebuffer_attribute* fbAttrib = (struct ultra_framebuffer_attribute*) curAttribute;

			const char* formatType;
			switch (fbAttrib->fb.format)
			{
			case ULTRA_FB_FORMAT_INVALID: formatType = "Invalid"; break;
			case ULTRA_FB_FORMAT_RGB888: formatType = "RGB"; break;
			case ULTRA_FB_FORMAT_BGR888: formatType = "BGR"; break;
			case ULTRA_FB_FORMAT_RGBX8888: formatType = "RGBA"; break;
			case ULTRA_FB_FORMAT_XRGB8888: formatType = "ARGB"; break;
			default: formatType = "Unknown"; break;
			}
			LogDebugFormatted("Ultra",
							  "Framebuffer %ux%u(%u) %hubpp %s %016lx",
							  fbAttrib->fb.width,
							  fbAttrib->fb.height,
							  fbAttrib->fb.pitch,
							  fbAttrib->fb.bpp,
							  formatType,
							  fbAttrib->fb.physical_address);
			break;
		}
		}
		curAttribute = ULTRA_NEXT_ATTRIBUTE(curAttribute);
	}
#endif
}