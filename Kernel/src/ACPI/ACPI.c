#include "ACPI/ACPI.h"
#include "ACPI/Tables.h"
#include "DebugCon.h"
#include "Log.h"

#include <stddef.h>

struct ACPIState
{
	void* RootTable;
	bool  IsXSDT;

	void*   LAPICAddress;
	void*   IOAPICAddress;
	uint8_t LapicCount;
	uint8_t LAPICIDs[256];
};

struct ACPIState g_ACPIState;

static uint8_t ACPIChecksum(const void* data, size_t count)
{
	const uint8_t* pD = (const uint8_t*) data;
	uint8_t        r  = 0;
	for (size_t i = 0; i < count; ++i)
		r += pD[i];
	return r;
}

static void VisitFADT(struct ACPI_FADT* fadt, uint32_t index);
static void VisitMADT(struct ACPI_MADT* madt, uint32_t index);
static void VisitMADT_LAPIC(struct ACPI_MADT_LAPIC* lapic, uint32_t index);
static void VisitMADT_IO_APIC(struct ACPI_MADT_IO_APIC* ioapic, uint32_t index);
static void VisitMADT_ISO(struct ACPI_MADT_ISO* iso, uint32_t index);
static void VisitMADT_NMIS(struct ACPI_MADT_NMIS* nmis, uint32_t index);
static void VisitMADT_LAPIC_NMI(struct ACPI_MADT_LAPIC_NMI* lnmi, uint32_t index);
static void VisitMADT_LAPIC_AO(struct ACPI_MADT_LAPIC_AO* ao, uint32_t index);
static void VisitMADT_IO_SAPIC(struct ACPI_MADT_IO_SAPIC* sapic, uint32_t index);
static void VisitMADT_LSAPIC(struct ACPI_MADT_LSAPIC* lsapic, uint32_t index);
static void VisitMADT_PIS(struct ACPI_MADT_PIS* pis, uint32_t index);
static void VisitMADT_Lx2APIC(struct ACPI_MADT_Lx2APIC* lx2apic, uint32_t index);
static void VisitMADT_Lx2APIC_NMI(struct ACPI_MADT_Lx2APIC_NMI* lx2nmi, uint32_t index);
static void VisitMADT_GICC(struct ACPI_MADT_GICC* gicc, uint32_t index);
static void VisitMADT_GICD(struct ACPI_MADT_GICD* gicd, uint32_t index);
static void VisitMADT_GIC_MSI_FRAME(struct ACPI_MADT_GIC_MSI_FRAME* gicmf, uint32_t index);
static void VisitMADT_GICR(struct ACPI_MADT_GICR* gicr, uint32_t index);
static void VisitMADT_GIC_ITS(struct ACPI_MADT_GIC_ITS* gic, uint32_t index);
static void VisitMADT_MULTIPROCESSOR_WAKEUP(struct ACPI_MADT_MULTIPROCESSOR_WAKEUP* mpwake, uint32_t index);
static void VisitMADT_CORE_PIC(struct ACPI_MADT_CORE_PIC* cpic, uint32_t index);
static void VisitMADT_LIO_PIC(struct ACPI_MADT_LIO_PIC* lpic, uint32_t index);
static void VisitMADT_HT_PIC(struct ACPI_MADT_HT_PIC* hpic, uint32_t index);
static void VisitMADT_EIO_PIC(struct ACPI_MADT_EIO_PIC* epic, uint32_t index);
static void VisitMADT_MSI_PIC(struct ACPI_MADT_MSI_PIC* mpic, uint32_t index);
static void VisitMADT_BIO_PIC(struct ACPI_MADT_BIO_PIC* bpic, uint32_t index);
static void VisitMADT_LPC_PIC(struct ACPI_MADT_LPC_PIC* lpic, uint32_t index);
static void VisitDescriptionTable(struct ACPI_DESC_HEADER* header, uint32_t index);

void HandleACPITables(void* rsdpAddress)
{
	g_ACPIState = (struct ACPIState) {
		.RootTable     = nullptr,
		.IsXSDT        = false,
		.LAPICAddress  = nullptr,
		.IOAPICAddress = nullptr,
		.LapicCount    = 0
	};

	LogLock();
	struct ACPI_RSDP* rsdp = (struct ACPI_RSDP*) rsdpAddress;
	if (*(uint64_t*) &rsdp->Signature != 0x2052'5450'2044'5352UL)
	{
		LogCriticalFormatted("ACPI", "RSDP contains invalid signature '%.8s', expected 'RSD PTR '", (const char*) rsdp->Signature);
		LogUnlock();
		return; // TODO(MarcasRealAccount): PANIC
	}
	uint8_t checksum = ACPIChecksum(rsdp, 20);
	if (checksum != 0)
	{
		LogCriticalFormatted("ACPI", "RSDP checksum is not valid '%02hhX', expected '%02hhX'", rsdp->Checksum, -(checksum - rsdp->Checksum));
		LogUnlock();
		return; // TODO(MarcasRealAccount): PANIC
	}

	LogDebugFormatted("ACPI", "RSDP Address: 0x%016lX", (uint64_t) rsdp);

	bool useXSDT = false;
	if (rsdp->Revision > 1)
	{
		checksum = ACPIChecksum(rsdp, 36);
		if (checksum == 0)
			useXSDT = true;
		else
			LogDebugFormatted("ACPI", " RSDP Extended checksum is not valid '%02hhX', expected '%02hhX' falling back to RSDT", rsdp->ExtendedChecksum, -(checksum - rsdp->ExtendedChecksum));
	}

	if (useXSDT)
	{
		struct ACPI_XSDT* xsdt = (struct ACPI_XSDT*) rsdp->XsdtAddress;
		if (*(uint32_t*) &xsdt->Header.Signature != 'TDSX')
		{
			LogCriticalFormatted("ACPI", "XSDT contains invalid signature '%.4s', expected 'XSDT'", (const char*) xsdt->Header.Signature);
			LogUnlock();
			return; // TODO(MarcasRealAccount): PANIC
		}
		checksum = ACPIChecksum(xsdt, xsdt->Header.Length);
		if (checksum != 0)
		{
			LogCriticalFormatted("ACPI", "XSDT checksum is not valid '%02hhX', expected '%02hhX'", xsdt->Header.Checksum, -(checksum - xsdt->Header.Checksum));
			LogUnlock();
			return; // TODO(MarcasRealAccount): PANIC
		}
		uint32_t entryCount   = (xsdt->Header.Length - 36) / 8;
		g_ACPIState.RootTable = xsdt;
		g_ACPIState.IsXSDT    = true;

		LogDebugFormatted("ACPI", "XSDT Address: 0x%016lX, Length: %u, Entry Count: %u", (uint64_t) xsdt, xsdt->Header.Length, entryCount);
		for (uint32_t i = 0; i < entryCount; ++i)
			VisitDescriptionTable((struct ACPI_DESC_HEADER*) xsdt->Entries[i], i);
	}
	else
	{
		struct ACPI_RSDT* rsdt = (struct ACPI_RSDT*) (uint64_t) rsdp->RsdtAddress;
		if (*(uint32_t*) &rsdt->Header.Signature != 'TDSR')
		{
			LogCriticalFormatted("ACPI", "RSDT contains invalid signature '%.4s', expected 'RSDT'", (const char*) rsdt->Header.Signature);
			LogUnlock();
			return; // TODO(MarcasRealAccount): PANIC
		}
		checksum = ACPIChecksum(rsdt, rsdt->Header.Length);
		if (checksum != 0)
		{
			LogCriticalFormatted("ACPI", "RSDT checksum is not valid '%02hhX', expected '%02hhX'", rsdt->Header.Checksum, -(checksum - rsdt->Header.Checksum));
			LogUnlock();
			return; // TODO(MarcasRealAccount): PANIC
		}
		uint32_t entryCount   = (rsdt->Header.Length - 36) / 4;
		g_ACPIState.RootTable = rsdt;
		g_ACPIState.IsXSDT    = false;

		LogDebugFormatted("ACPI", "RSDT Address: 0x%016lX, Length: %u, Entry Count: %u", (uint64_t) rsdt, rsdt->Header.Length, entryCount);
		for (uint32_t i = 0; i < entryCount; ++i)
			VisitDescriptionTable((struct ACPI_DESC_HEADER*) (uint64_t) rsdt->Entries[i], i);
	}

	LogDebugFormatted("ACPI",
					  "Detected %hhu Local APICs, IO APIC Address 0x%016lX, Local APIC Address 0x%016lX",
					  g_ACPIState.LapicCount,
					  (uint64_t) g_ACPIState.IOAPICAddress,
					  (uint64_t) g_ACPIState.LAPICAddress);
	LogUnlock();
}

void* GetLAPICAddress(void)
{
	return g_ACPIState.LAPICAddress;
}

void* GetIOAPICAddress(void)
{
	return g_ACPIState.IOAPICAddress;
}

uint8_t* GetLAPICIDs(uint8_t* lapicCount)
{
	if (!lapicCount)
		return nullptr;
	*lapicCount = g_ACPIState.LapicCount;
	return g_ACPIState.LAPICIDs;
}

static const char* c_FADTPPMPStrs[] = { "Unspecified", "Desktop", "Mobile", "Workstation", "Enterprise", "SOHO", "Appliance", "Performance", "Tablet" };

static const char* c_FADTStr = "  Flags:\n"
							   "   WBINVD:                               %b\n"
							   "   WBINVD_FLUSH:                         %b\n"
							   "   PROC_C1:                              %b\n"
							   "   PLVL2_UP:                             %b\n"
							   "   POWER_BUTTON:                         %b\n"
							   "   SLEEP_BUTTON:                         %b\n"
							   "   FIX_RTC:                              %b\n"
							   "   RTC_S4:                               %b\n"
							   "   TIMER_VAL_EXTENDED:                   %b\n"
							   "   DOCK_CAPABLE:                         %b\n"
							   "   RESET_REG_SUPPORTED:                  %b\n"
							   "   SEALED_CASE:                          %b\n"
							   "   HEADLESS:                             %b\n"
							   "   CPU_SW_SLEEP:                         %b\n"
							   "   PCI_EXP_WAK:                          %b\n"
							   "   USER_PLATFORM_CLOCK:                  %b\n"
							   "   S4_RTC_STS_VALID:                     %b\n"
							   "   REMOTE_POWER_ON_CAPABLE:              %b\n"
							   "   FORCE_APIC_CLUSTER_MODEL:             %b\n"
							   "   FORCE_APIC_PHYSICAL_DESTINATION_MODE: %b\n"
							   "   HW_REDUCED_ACPI:                      %b\n"
							   "   LOW_POWER_S0_IDLE_CAPABLE:            %b\n"
							   "   PERSISTENT_CPU_CACHES:                %b\n"
							   "  Preferred Power Manager Profile: %s\n"
							   "  SCI Interrupt:    0x%04hX\n"
							   "  SMI Command Port: 0x%04X\n"
							   "   S4BIOS Request: %02hhX\n"
							   "   PState Control: %02hhX\n"
							   "   CST Control:    %02hhX\n"
							   "  PM1 Event Block a Address: 0x%08X, b Address: 0x%08X, Length: %hhu\n"
							   "  PM1 Control Block a Address: 0x%08X, b Address: 0x%08X, Length: %hhu\n"
							   "  PM2 Control Block Address: 0x%08X, Length: %hhu\n"
							   "  PM Timer Block Address: 0x%08X, Length: %hhu\n"
							   "  GP Event Block 1 Address: 0x%08X, Length: %hhu, 2 Address: 0x%08X, Base: %hhu, Length %hhu\n"
							   "  P LVL2 Latency: %hu\n"
							   "  P LVL3 Latency: %hu\n"
							   "  Cache flush Size: %hu, Stride: %hu\n"
							   "  Duty Offset: %hhu, Width %hhu\n"
							   "  RTC CMOS:\n"
							   "   Day Alarm index:   %hhu\n"
							   "   Month Alarm index: %hhu\n"
							   "   Century index:     %hhu\n"
							   "  Hypervisor Vendor Identity: %016lX\n"
							   "  FACS Address: 0x%016lX, Signature '%.4s', Hardware Signature '%.4s', Version %hhu:\n"
							   "   Flags:\n"
							   "    S4BIOS_F:               %b\n"
							   "    64BIT_WAKE_SUPPORTED_F: %b\n"
							   "  DSDT Address: 0x%016lX, Length: %u";

void VisitFADT(struct ACPI_FADT* fadt, uint32_t index)
{
	struct ACPI_FACS* facs = (struct ACPI_FACS*) (fadt->XFirmwareControl ? fadt->XFirmwareControl : (uint64_t) fadt->FirmwareControl);
	struct ACPI_DSDT* dsdt = (struct ACPI_DSDT*) (fadt->XDSDT ? fadt->XDSDT : (uint64_t) fadt->DSDT);
	LogDebugFormatted("ACPI",
					  c_FADTStr,
					  fadt->Flags & ACPI_FADT_WBINVD_MASK,
					  fadt->Flags & ACPI_FADT_WBINVD_FLUSH_MASK,
					  fadt->Flags & ACPI_FADT_PROC_C1_MASK,
					  fadt->Flags & ACPI_FADT_PLVL2_UP_MASK,
					  fadt->Flags & ACPI_FADT_POWER_BUTTON_MASK,
					  fadt->Flags & ACPI_FADT_SLEEP_BUTTON_MASK,
					  fadt->Flags & ACPI_FADT_FIX_RTC_MASK,
					  fadt->Flags & ACPI_FADT_RTC_S4_MASK,
					  fadt->Flags & ACPI_FADT_TIMER_VAL_EXTENDED_MASK,
					  fadt->Flags & ACPI_FADT_DOCK_CAPABLE_MASK,
					  fadt->Flags & ACPI_FADT_RESET_REG_SUPPORTED_MASK,
					  fadt->Flags & ACPI_FADT_SEALED_CASE_MASK,
					  fadt->Flags & ACPI_FADT_HEADLESS_MASK,
					  fadt->Flags & ACPI_FADT_CPU_SW_SLEEP_MASK,
					  fadt->Flags & ACPI_FADT_PCI_EXP_WAK_MASK,
					  fadt->Flags & ACPI_FADT_USER_PLATFORM_CLOCK_MASK,
					  fadt->Flags & ACPI_FADT_S4_RTC_STS_VALID_MASK,
					  fadt->Flags & ACPI_FADT_REMOTE_POWER_ON_CAPABLE_MASK,
					  fadt->Flags & ACPI_FADT_FORCE_APIC_CLUSTER_MODEL_MASK,
					  fadt->Flags & ACPI_FADT_FORCE_APIC_PHYSICAL_DESTINATION_MODE_MASK,
					  fadt->Flags & ACPI_FADT_HW_REDUCED_ACPI_MASK,
					  fadt->Flags & ACPI_FADT_LOW_POWER_S0_IDLE_CAPABLE_MASK,
					  fadt->Flags & ACPI_FADT_PERSISTENT_CPU_CACHES_MASK,
					  fadt->PreferredPMProfile < 8 ? c_FADTPPMPStrs[fadt->PreferredPMProfile] : "Reserved",
					  fadt->SCIInt,
					  fadt->SMICmd,
					  fadt->S4BIOSReq,
					  fadt->PStateControl,
					  fadt->CSTControl,
					  fadt->PM1aEventBlock,
					  fadt->PM1bEventBlock,
					  fadt->PM1EventLength,
					  fadt->PM1aControlBlock,
					  fadt->PM1bControlBlock,
					  fadt->PM1ControlLength,
					  fadt->PM2ControlBlock,
					  fadt->PM2ControlLength,
					  fadt->PMTimerBlock,
					  fadt->PMTimerLength,
					  fadt->GPE0Block,
					  fadt->GPE0BlockLength,
					  fadt->GPE1Block,
					  fadt->GPE1Base,
					  fadt->GPE1BlockLength,
					  fadt->PLVL2Latency,
					  fadt->PLVL3Latency,
					  fadt->FlushSize,
					  fadt->FlushStride,
					  fadt->DutyOffset,
					  fadt->DutyWidth,
					  fadt->DayAlarm,
					  fadt->MonthAlarm,
					  fadt->Century,
					  *(uint64_t*) fadt->HypervisorVendorIdentity,
					  (uint64_t) facs,
					  (const char*) facs->Signature,
					  (const char*) facs->HardwareSignature,
					  facs->Version,
					  facs->Flags & ACPI_FACS_S4BIOS_F_MASK,
					  facs->Flags & ACPI_FACS_64BIT_WAKE_SUPPORTED_MASK,
					  (uint64_t) dsdt,
					  dsdt->Header.Length - 36);
	uint32_t checksum = ACPIChecksum(dsdt, dsdt->Header.Length);
	if (checksum != 0)
		LogWarnFormatted("ACPI", "DSDT Address: 0x%016lX has invalid checksum '%02hhX', expected '%02hhX' skipping", (uint64_t) dsdt, dsdt->Header.Checksum, -(checksum - fadt->Header.Checksum));
}

static const char* c_MADTTypeStrs[] = { "Processor Local APIC", "IO APIC", "ISO", "NMI Source", "Local APIC NMI", "Local APIC Address Override", "IO SAPIC", "Local SAPIC", "Platform Interrupt Sources", "Processor Local x2APIC", "Local x2APIC NMI", "GIC CPU Interface", "GIC Distributor", "GIC MSI Frame", "GIC Redistributor", "GIC Interrupt Translation", "Multiprocessor Wakeup", "CORE PIC", "LIO PIC", "HT PIC", "EIO PIC", "MSI PIC", "BIO PIC", "LPC PIC" };

static const char* c_MADTStr = "  Flags:\n"
							   "   PCAT_COMPAT: %hhu\n"
							   "  Local Interrupt Controller Address: 0x%08X";

void VisitMADT(struct ACPI_MADT* madt, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_MADTStr,
					  madt->Flags & ACPI_MADT_PCAT_COMPAT_MASK,
					  madt->LICA);

	g_ACPIState.LAPICAddress = (void*) (uint64_t) madt->LICA;

	uint32_t                    icEntry = 0;
	size_t                      offset  = 44;
	struct ACPI_MADT_IC_HEADER* curIC   = madt->Entries;
	while (offset < madt->Header.Length)
	{
		uint8_t len = curIC->Length;
		LogDebugFormatted("ACPI", "  Entry %u, Type: '%s', Length: %hhu:", icEntry, curIC->Type < 17 ? c_MADTTypeStrs[curIC->Type] : "Unknown", curIC->Length);
		switch (curIC->Type)
		{
		case ACPI_MADT_IC_PROCESSOR_LAPIC: VisitMADT_LAPIC((struct ACPI_MADT_LAPIC*) curIC, icEntry); break;
		case ACPI_MADT_IC_IOAPIC: VisitMADT_IO_APIC((struct ACPI_MADT_IO_APIC*) curIC, icEntry); break;
		case ACPI_MADT_IC_ISO: VisitMADT_ISO((struct ACPI_MADT_ISO*) curIC, icEntry); break;
		case ACPI_MADT_IC_NMIS: VisitMADT_NMIS((struct ACPI_MADT_NMIS*) curIC, icEntry); break;
		case ACPI_MADT_IC_LAPIC_NMI: VisitMADT_LAPIC_NMI((struct ACPI_MADT_LAPIC_NMI*) curIC, icEntry); break;
		case ACPI_MADT_IC_LAPIC_ADDRESS_OVERRIDE: VisitMADT_LAPIC_AO((struct ACPI_MADT_LAPIC_AO*) curIC, icEntry); break;
		case ACPI_MADT_IC_IO_SAPIC: VisitMADT_IO_SAPIC((struct ACPI_MADT_IO_SAPIC*) curIC, icEntry); break;
		case ACPI_MADT_IC_LSAPIC: VisitMADT_LSAPIC((struct ACPI_MADT_LSAPIC*) curIC, icEntry); break;
		case ACPI_MADT_IC_PIS: VisitMADT_PIS((struct ACPI_MADT_PIS*) curIC, icEntry); break;
		case ACPI_MADT_IC_PROCESSOR_Lx2APIC: VisitMADT_Lx2APIC((struct ACPI_MADT_Lx2APIC*) curIC, icEntry); break;
		case ACPI_MADT_IC_Lx2APIC_NMI: VisitMADT_Lx2APIC_NMI((struct ACPI_MADT_Lx2APIC_NMI*) curIC, icEntry); break;
		case ACPI_MADT_IC_GICC: VisitMADT_GICC((struct ACPI_MADT_GICC*) curIC, icEntry); break;
		case ACPI_MADT_IC_GICD: VisitMADT_GICD((struct ACPI_MADT_GICD*) curIC, icEntry); break;
		case ACPI_MADT_IC_GIC_MSI_FRAME: VisitMADT_GIC_MSI_FRAME((struct ACPI_MADT_GIC_MSI_FRAME*) curIC, icEntry); break;
		case ACPI_MADT_IC_GICR: VisitMADT_GICR((struct ACPI_MADT_GICR*) curIC, icEntry); break;
		case ACPI_MADT_IC_GIC_ITS: VisitMADT_GIC_ITS((struct ACPI_MADT_GIC_ITS*) curIC, icEntry); break;
		case ACPI_MADT_IC_MULTIPROCESSOR_WAKEUP: VisitMADT_MULTIPROCESSOR_WAKEUP((struct ACPI_MADT_MULTIPROCESSOR_WAKEUP*) curIC, icEntry); break;
		case ACPI_MADT_IC_CORE_PIC: VisitMADT_CORE_PIC((struct ACPI_MADT_CORE_PIC*) curIC, icEntry); break;
		case ACPI_MADT_IC_LIO_PIC: VisitMADT_LIO_PIC((struct ACPI_MADT_LIO_PIC*) curIC, icEntry); break;
		case ACPI_MADT_IC_HT_PIC: VisitMADT_HT_PIC((struct ACPI_MADT_HT_PIC*) curIC, icEntry); break;
		case ACPI_MADT_IC_EIO_PIC: VisitMADT_EIO_PIC((struct ACPI_MADT_EIO_PIC*) curIC, icEntry); break;
		case ACPI_MADT_IC_MSI_PIC: VisitMADT_MSI_PIC((struct ACPI_MADT_MSI_PIC*) curIC, icEntry); break;
		case ACPI_MADT_IC_BIO_PIC: VisitMADT_BIO_PIC((struct ACPI_MADT_BIO_PIC*) curIC, icEntry); break;
		case ACPI_MADT_IC_LPC_PIC: VisitMADT_LPC_PIC((struct ACPI_MADT_LPC_PIC*) curIC, icEntry); break;
		default: LogDebugFormatted("ACPI", "  Entry %u, Type: %02hhX, Length: %hhu Unknown", icEntry, curIC->Type, curIC->Length); break;
		}
		curIC   = (struct ACPI_MADT_IC_HEADER*) ((uint8_t*) curIC + len);
		offset += len;
		++icEntry;
	}
}

static const char* c_LAPICStr = "   Flags:\n"
								"    Enabled:        %b\n"
								"    Online Capable: %b\n"
								"   Processor UID: %02hhX\n"
								"   Processor ID:  %02hhX";

void VisitMADT_LAPIC(struct ACPI_MADT_LAPIC* lapic, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_LAPICStr,
					  lapic->Flags & ACPI_MADT_LAPIC_ENABLED_MASK,
					  lapic->Flags & ACPI_MADT_LAPIC_ONLINE_CAPABLE_MASK,
					  lapic->ACPIProcessorUID,
					  lapic->APICID);

	switch (lapic->Flags & (ACPI_MADT_LAPIC_ENABLED_MASK | ACPI_MADT_LAPIC_ONLINE_CAPABLE_MASK))
	{
	case 0: LogDebugFormatted("ACPI", "   -> Not usable"); break;
	case ACPI_MADT_LAPIC_ENABLED_MASK:
		LogDebugFormatted("ACPI", "   -> Enabled");
		g_ACPIState.LAPICIDs[g_ACPIState.LapicCount++] = lapic->APICID;
		break;
	case ACPI_MADT_LAPIC_ONLINE_CAPABLE_MASK: LogDebugFormatted("ACPI", "   -> Can be Enabled"); break;
	case ACPI_MADT_LAPIC_ENABLED_MASK | ACPI_MADT_LAPIC_ONLINE_CAPABLE_MASK: LogDebugFormatted("ACPI", "   -> Weird quasi state"); break;
	}
}

static const char* c_IOAPICSStr = "   ID:      %02hhX\n"
								  "   Address: %08X\n"
								  "   GSIB:    %08X";

void VisitMADT_IO_APIC(struct ACPI_MADT_IO_APIC* ioapic, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_IOAPICSStr,
					  ioapic->IOAPICID,
					  ioapic->IOAPICAddress,
					  ioapic->GSIB);

	g_ACPIState.IOAPICAddress = (void*) (uint64_t) ioapic->IOAPICAddress;
}

static const char* c_PolarityStrs[4]    = { "Conformant", "Active High", "Reserved", "Active Low" };
static const char* c_TriggerModeStrs[4] = { "Conformant", "Edge-triggered", "Reserved", "Level-triggered" };

static const char* c_ISOStr = "   Flags:\n"
							  "    Polarity:     %s\n"
							  "    Trigger mode: %s\n"
							  "   Bus:    %02hhX\n"
							  "   Source: %02hhX\n"
							  "   GSI:    %08X";

void VisitMADT_ISO(struct ACPI_MADT_ISO* iso, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_ISOStr,
					  c_PolarityStrs[(iso->Flags & ACPI_MADT_ISO_POLARITY_MASK) >> ACPI_MADT_ISO_POLARITY_BIT],
					  c_TriggerModeStrs[(iso->Flags & ACPI_MADT_ISO_TRIGGER_MODE_MASK) >> ACPI_MADT_ISO_TRIGGER_MODE_MASK],
					  iso->Bus,
					  iso->Source,
					  iso->GSI);
}

static const char* c_NMISStr = "   Flags:\n"
							   "    Polarity:     %s\n"
							   "    Trigger mode: %s\n"
							   "   GSI: %08X";

void VisitMADT_NMIS(struct ACPI_MADT_NMIS* nmis, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_NMISStr,
					  c_PolarityStrs[nmis->Flags & ACPI_MADT_ISO_POLARITY_MASK],
					  c_TriggerModeStrs[nmis->Flags & ACPI_MADT_ISO_TRIGGER_MODE_MASK],
					  nmis->GSI);
}

static const char* c_LAPIC_NMIStr = "   Flags:\n"
									"    Polarity:     %s\n"
									"    Trigger mode: %s\n"
									"   Processor UID:    %08X\n"
									"   Local APIC LINT#: %02hhX";

void VisitMADT_LAPIC_NMI(struct ACPI_MADT_LAPIC_NMI* nmi, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_LAPIC_NMIStr,
					  c_PolarityStrs[nmi->Flags & ACPI_MADT_ISO_POLARITY_MASK],
					  c_TriggerModeStrs[nmi->Flags & ACPI_MADT_ISO_TRIGGER_MODE_MASK],
					  nmi->ACPIProcessorUID,
					  nmi->LAPIC_LINT);
}

static const char* c_LAPIC_AOStr = "   Address: 0x%016lX";

void VisitMADT_LAPIC_AO(struct ACPI_MADT_LAPIC_AO* ao, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_LAPIC_AOStr,
					  ao->APICAddress);

	g_ACPIState.LAPICAddress = (void*) ao->APICAddress;
}

static const char* c_IOSAPICStr = "   IO ID:      %02hhX\n"
								  "   GIS Base:   %08X\n"
								  "   IO Address: 0x%016lX";

void VisitMADT_IO_SAPIC(struct ACPI_MADT_IO_SAPIC* sapic, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_IOSAPICStr,
					  sapic->IOAPICID,
					  sapic->GSIB,
					  sapic->IOSAPICAddress);
}

static const char* c_LSAPICStr = "   Flags:\n"
								 "    Enabled:        %b\n"
								 "    Online Capable: %b\n"
								 "   Local ID:      %02hhX\n"
								 "   Local EID:     %02hhX\n"
								 "   Processor UID: %08X '%.*s'\n"
								 "   ID:            %02hhX";

void VisitMADT_LSAPIC(struct ACPI_MADT_LSAPIC* lsapic, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_LSAPICStr,
					  lsapic->Flags & ACPI_MADT_LAPIC_ENABLED_MASK,
					  lsapic->Flags & ACPI_MADT_LAPIC_ONLINE_CAPABLE_MASK,
					  lsapic->LSAPICEID,
					  lsapic->LSAPICEID,
					  lsapic->ACPIProcessorUID,
					  lsapic->Header.Length - 16,
					  lsapic->ACPIProcessorUIDStr,
					  lsapic->ACPIProcessorID);
}

static const char* c_PISInterruptTypeStrs[] = { "Unknown", "PMI", "INIT", "Corrected Platform Error Interrupt" };

static const char* c_PISStr = "   Flags:\n"
							  "    Polarity:     %s\n"
							  "    Trigger mode: %s\n"
							  "   Platform Interrupt Source Flags:\n"
							  "    CPEI Processor Override: %b\n"
							  "   Type:            %s\n"
							  "   Processor ID:    %02hhX\n"
							  "   Processor EID:   %02hhX\n"
							  "   IO SAPIC Vector: %02hhX\n"
							  "   GSI:             %08X";

void VisitMADT_PIS(struct ACPI_MADT_PIS* pis, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_PISStr,
					  pis->Flags & ACPI_MADT_ISO_POLARITY_MASK,
					  pis->Flags & ACPI_MADT_ISO_TRIGGER_MODE_MASK,
					  pis->PISFlags & ACPI_MADT_PIS_CPEI_PROCESSOR_OVERRIDE_MASK,
					  pis->InterruptType < 4 ? c_PISInterruptTypeStrs[pis->InterruptType] : "Unknown",
					  pis->ProcessorID,
					  pis->ProcessorEID,
					  pis->IOSAPICVector,
					  pis->GSI);
}

static const char* c_Lx2APICStr = "   Flags:\n"
								  "    Enabled:        %b\n"
								  "    Online Capable: %b\n"
								  "   Processor UID: %08X '%.*s'\n"
								  "   X2 ID:         %02hhX";

void VisitMADT_Lx2APIC(struct ACPI_MADT_Lx2APIC* lx2apic, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_Lx2APICStr,
					  lx2apic->Flags & ACPI_MADT_LAPIC_ENABLED_MASK,
					  lx2apic->Flags & ACPI_MADT_LAPIC_ONLINE_CAPABLE_MASK,
					  lx2apic->ACPIProcessorUID,
					  lx2apic->X2APICID);
}

static const char* c_Lx2APIC_NMIStr = "   Flags:\n"
									  "    Polarity:     %s\n"
									  "    Trigger mode: %s\n"
									  "   Processor UID:      %08X\n"
									  "   Local x2APIC LINT#: %02hhX";

void VisitMADT_Lx2APIC_NMI(struct ACPI_MADT_Lx2APIC_NMI* lx2nmi, uint32_t index)
{
	LogDebugFormatted("ACPI",
					  c_Lx2APIC_NMIStr,
					  c_PolarityStrs[(lx2nmi->Flags & ACPI_MADT_ISO_POLARITY_MASK) >> ACPI_MADT_ISO_POLARITY_BIT],
					  c_TriggerModeStrs[(lx2nmi->Flags & ACPI_MADT_ISO_TRIGGER_MODE_MASK) >> ACPI_MADT_ISO_TRIGGER_MODE_BIT],
					  lx2nmi->ACPIProcessorUID,
					  lx2nmi->Lx2APIC_LINT);
}

void VisitMADT_GICC(struct ACPI_MADT_GICC* gicc, uint32_t index)
{
}

void VisitMADT_GICD(struct ACPI_MADT_GICD* gicd, uint32_t index)
{
}

void VisitMADT_GIC_MSI_FRAME(struct ACPI_MADT_GIC_MSI_FRAME* gicmf, uint32_t index)
{
}

void VisitMADT_GICR(struct ACPI_MADT_GICR* gicr, uint32_t index)
{
}

void VisitMADT_GIC_ITS(struct ACPI_MADT_GIC_ITS* gic, uint32_t index)
{
}

void VisitMADT_MULTIPROCESSOR_WAKEUP(struct ACPI_MADT_MULTIPROCESSOR_WAKEUP* mpwake, uint32_t index)
{
}

void VisitMADT_CORE_PIC(struct ACPI_MADT_CORE_PIC* cpic, uint32_t index)
{
}

void VisitMADT_LIO_PIC(struct ACPI_MADT_LIO_PIC* lpic, uint32_t index)
{
}

void VisitMADT_HT_PIC(struct ACPI_MADT_HT_PIC* hpic, uint32_t index)
{
}

void VisitMADT_EIO_PIC(struct ACPI_MADT_EIO_PIC* epic, uint32_t index)
{
}

void VisitMADT_MSI_PIC(struct ACPI_MADT_MSI_PIC* mpic, uint32_t index)
{
}

void VisitMADT_BIO_PIC(struct ACPI_MADT_BIO_PIC* bpic, uint32_t index)
{
}

void VisitMADT_LPC_PIC(struct ACPI_MADT_LPC_PIC* lpic, uint32_t index)
{
}

void VisitDescriptionTable(struct ACPI_DESC_HEADER* header, uint32_t index)
{
	uint32_t checksum = ACPIChecksum(header, header->Length);
	if (checksum != 0)
	{
		LogDebugFormatted("ACPI",
						  " Entry %u, Address: 0x%016lX, Signature: '%.4s' has invalid checksum '%02hhX', expected '%02hhX' skipping",
						  index,
						  (uint64_t) header,
						  (const char*) header->Signature,
						  header->Checksum,
						  -(checksum - header->Checksum));
		return;
	}

	switch (*(uint32_t*) &header->Signature)
	{
	case 'PCAF':
		LogDebugFormatted("ACPI", " Entry %u, Address: 0x%016lX, Signature: 'FACP' Fixed APIC Description Table:", index, (uint64_t) header);
		VisitFADT((struct ACPI_FADT*) header, index);
		break;
	case 'CIPA':
		LogDebugFormatted("ACPI", " Entry %u, Address: 0x%016lX, Signature: 'APIC' Multiple APIC Description Table:", index, (uint64_t) header);
		VisitMADT((struct ACPI_MADT*) header, index);
		break;
	default:
		LogDebugFormatted("ACPI", " Entry %u, Address: 0x%016lX, Signature: '%.4s' unused skipping", index, (uint64_t) header, (const char*) header->Signature);
		break;
	}
}