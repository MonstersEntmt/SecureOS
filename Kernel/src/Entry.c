#include "DebugCon.h"
#include "DescriptorTables.h"
#include "Ultra/UltraProtocol.h"

static void VisitUltraInvalidAttribute(void* userdata);
static void VisitUltraPlatformInfoAttribute(struct ultra_platform_info_attribute* attribute, void* userdata);
static void VisitUltraKernelInfoAttribute(struct ultra_kernel_info_attribute* attribute, void* userdata);
static void VisitUltraMemoryMapAttribute(struct ultra_memory_map_attribute* attribute, void* userdata);
static void VisitUltraModuleInfoAttribute(struct ultra_module_info_attribute* attribute, void* userdata);
static void VisitUltraCommandLineAttribute(struct ultra_command_line_attribute* attribute, void* userdata);
static void VisitUltraFramebufferAttribute(struct ultra_framebuffer_attribute* attribute, void* userdata);
static void VisitUltraAttribute(struct ultra_attribute_header* attribute, void* userdata);

// __attribute__((interrupt)) static void DefaultInterruptHandler(struct InterruptHandlerData* data);
void DefaultInterruptHandler(struct InterruptHandlerData* data);

extern void DefaultInterruptHandlerWrapper(struct InterruptHandlerData* data);

void kernel_entry(struct ultra_boot_context* bootContext, uint32_t magic)
{
	if (magic != ULTRA_MAGIC)
	{
		DebugCon_WriteFormatted("Entry magic %08X is not %08X\r\n", magic, ULTRA_MAGIC);
		return;
	}

	SetCodeSegment(0, 0, false);
	SetDataSegment(1);
	for (size_t i = 0; i < 256; ++i)
		SetInterruptHandler((uint8_t) i, 0, 0, 0, DefaultInterruptHandlerWrapper);
	LoadDescriptorTables();

	// __asm__("int $49;");

	{
		struct ultra_attribute_header* curAttribute = bootContext->attributes;
		for (uint32_t i = 0; i < bootContext->attribute_count; ++i)
		{
			VisitUltraAttribute(curAttribute, nullptr);
			curAttribute = ULTRA_NEXT_ATTRIBUTE(curAttribute);
		}
	}

	__asm__(".l: hlt; jmp .l;");
}

// __attribute__((interrupt)) void DefaultInterruptHandler(struct InterruptHandlerData* data)
// {
// 	// DebugCon_WriteFormatted("Interrupt\r\n");
// }
void DefaultInterruptHandler(struct InterruptHandlerData* data)
{
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

void VisitUltraMemoryMapAttribute(struct ultra_memory_map_attribute* attribute, void* userdata)
{
	DebugCon_WriteString("Memory Map:\r\n");
	size_t entryCount = ULTRA_MEMORY_MAP_ENTRY_COUNT(attribute->header);
	for (size_t i = 0; i < entryCount; ++i)
	{
		struct ultra_memory_map_entry* entry = attribute->entries + i;
		const char*                    memoryType;
		switch (entry->type)
		{
		case ULTRA_MEMORY_TYPE_INVALID: memoryType = "Invalid:"; break;
		case ULTRA_MEMORY_TYPE_FREE: memoryType = "Free:"; break;
		case ULTRA_MEMORY_TYPE_RESERVED: memoryType = "Reserved:"; break;
		case ULTRA_MEMORY_TYPE_RECLAIMABLE: memoryType = "Reclaimable:"; break;
		case ULTRA_MEMORY_TYPE_NVS: memoryType = "NVS:"; break;
		case ULTRA_MEMORY_TYPE_LOADER_RECLAIMABLE: memoryType = "Loader Reclaimable:"; break;
		case ULTRA_MEMORY_TYPE_MODULE: memoryType = "Module:"; break;
		case ULTRA_MEMORY_TYPE_KERNEL_STACK: memoryType = "Kernel Stack:"; break;
		case ULTRA_MEMORY_TYPE_KERNEL_BINARY: memoryType = "Kernel Binary:"; break;
		default: memoryType = "Unknown:"; break;
		}
		DebugCon_WriteFormatted("  %-19s %016lx -> %016lx(%lu)\r\n",
								memoryType,
								entry->physical_address,
								entry->physical_address + entry->size,
								entry->size);
	}
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