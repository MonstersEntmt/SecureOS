#include "ACPI/ACPI.h"
#include "Build.h"
#include "DebugCon.h"
#include "Graphics/Graphics.h"
#include "Halt.h"
#include "KernelVMM.h"
#include "PMM.h"
#include "Ultra/UltraProtocol.h"
#include "VMM.h"

#include <string.h>

#if BUILD_IS_ARCH_X86_64
	#include "x86_64/GDT.h"
	#include "x86_64/IDT.h"
	#include "x86_64/InterruptHandlers.h"
	#include "x86_64/Trampoline.h"
#endif

struct KernelStartupData
{
	void* RsdpAddress;

	struct Framebuffer Framebuffer;

	void* BasicLatin;
};

static void VisitUltraInvalidAttribute(struct KernelStartupData* userdata);
static void VisitUltraPlatformInfoAttribute(struct ultra_platform_info_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraKernelInfoAttribute(struct ultra_kernel_info_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraMemoryMapAttribute(struct ultra_memory_map_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraModuleInfoAttribute(struct ultra_module_info_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraCommandLineAttribute(struct ultra_command_line_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraFramebufferAttribute(struct ultra_framebuffer_attribute* attribute, struct KernelStartupData* userdata);
static void VisitUltraAttribute(struct ultra_attribute_header* attribute, struct KernelStartupData* userdata);

bool    g_LapicWaitLock = false;
uint8_t g_LapicsRunning = 0;
void    CPUTrampoline(uint8_t lapicID);
void*   CPUStackAlloc(void);

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

	LoadFont((struct FontHeader*) kernelStartupData.BasicLatin);

	{
		struct PMMMemoryStats memoryStats;
		PMMGetMemoryStats(&memoryStats);
		DebugCon_WriteFormatted("PMM Stats:\n  Footprint: %lu\n  Last Usable Address: 0x%016lu\n  Last Address: 0x%016lu\n  Pages Taken: %lu\n  Pages Free: %lu\n", (memoryStats.AllocatorFootprint + 4095) / 4096, memoryStats.LastUsableAddress, memoryStats.LastAddress, memoryStats.PagesTaken, memoryStats.PagesFree);
	}

	{
		struct VMMMemoryStats memoryStats;
		VMMGetMemoryStats(GetKernelPageTable(), &memoryStats);
		DebugCon_WriteFormatted("Kernel VMM Stats:\n  Footprint: %lu\n  Pages Allocated: %lu\n", (memoryStats.AllocatorFootprint + 4095) / 4096, memoryStats.PagesAllocated);
	}

	{
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
		uint8_t   lapicCount     = 0;
		uint8_t*  lapicIDs       = GetLAPICIDs(&lapicCount);

		g_LapicWaitLock = true;
		for (size_t i = 1; i < lapicCount; ++i) // Hoping implementations uphold the ACPI specification with first lapicID being the boot core
		{
			uint8_t id = lapicIDs[i];

			uint8_t pLapicsRunning = g_LapicsRunning;

			*trampolineStats = (struct x86_64TrampolineStats) {
				.Alive = 0,
				.Ack   = 0
			};

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
			trampolineStats->Ack = true;

			while (g_LapicsRunning == pLapicsRunning); // Synchronize with lapic bootstrap
		}
		g_LapicWaitLock = false;

		PMMFree(tempRootPageTable, 1);
	}

	GraphicsDrawRect(&kernelStartupData.Framebuffer, (struct GraphicsRect) { .x = 0, .y = 0, .w = kernelStartupData.Framebuffer.Width, .h = kernelStartupData.Framebuffer.Height }, (struct LinearColor) { .r = 0, .g = 0, .b = 0, .a = 65535 }, (struct LinearColor) { .r = 0, .g = 0, .b = 0, .a = 65535 });
	GraphicsDrawText(&kernelStartupData.Framebuffer, (struct GraphicsPoint) { .x = 5, .y = 5 }, "This is some test text\nWith multiple lines and with invalid characters \0\1\2\3\4 What'ya think?\nAnd also text\rover text,\ttabs too, but badly... Oh and also '\xFF' this :>", 163, (struct LinearColor) { .r = 65535, .g = 65535, .b = 65535, .a = 65535 });

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
	while (g_LapicWaitLock);

	DebugCon_WriteString("This string will look fucked\n");

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

	if (strcmp(attribute->name, "bsclatin.fnt") == 0)
		userdata->BasicLatin = (void*) attribute->address;
}

void VisitUltraCommandLineAttribute(struct ultra_command_line_attribute* attribute, struct KernelStartupData* userdata)
{
	DebugCon_WriteFormatted("Command line: %s\r\n", attribute->text);
}

void VisitUltraFramebufferAttribute(struct ultra_framebuffer_attribute* attribute, struct KernelStartupData* userdata)
{
	userdata->Framebuffer.Width      = attribute->fb.width;
	userdata->Framebuffer.Height     = attribute->fb.height;
	userdata->Framebuffer.Pitch      = attribute->fb.pitch;
	userdata->Framebuffer.Content    = (void*) attribute->fb.physical_address;
	userdata->Framebuffer.Colorspace = FramebufferColorspaceSRGB;

	const char* formatType;
	switch (attribute->fb.format)
	{
	case ULTRA_FB_FORMAT_INVALID: formatType = "Invalid"; break;
	case ULTRA_FB_FORMAT_RGB888:
		formatType                   = "RGB";
		userdata->Framebuffer.Format = FramebufferFormatRGB8;
		break;
	case ULTRA_FB_FORMAT_BGR888:
		formatType                   = "BGR";
		userdata->Framebuffer.Format = FramebufferFormatBGR8;
		break;
	case ULTRA_FB_FORMAT_RGBX8888:
		formatType                   = "RGBA";
		userdata->Framebuffer.Format = FramebufferFormatRGBA8;
		break;
	case ULTRA_FB_FORMAT_XRGB8888:
		formatType                   = "ARGB";
		userdata->Framebuffer.Format = FramebufferFormatARGB8;
		break;
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