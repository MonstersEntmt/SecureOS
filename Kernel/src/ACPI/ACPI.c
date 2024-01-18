#include "ACPI/ACPI.h"
#include "ACPI/Tables.h"
#include "DebugCon.h"
#include "KernelVMM.h"
#include "PMM.h"
#include "VMM.h"

#include <stddef.h>
#include <string.h>

struct ACPIState
{
	void* RootTable;
	bool  IsXSDT;
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

void HandleACPITables(void* rsdpAddress)
{
	struct ACPI_RSDP* rsdp = (struct ACPI_RSDP*) rsdpAddress;
	if (*(uint64_t*) &rsdp->Signature != 0x2052'5450'2044'5352UL)
	{
		DebugCon_WriteFormatted("RSDP contains invalid signature '%.8s', expected 'RSD PTR '\n", (const char*) rsdp->Signature);
		return; // TODO(MarcasRealAccount): PANIC
	}
	uint8_t checksum = ACPIChecksum(rsdp, 20);
	if (checksum != 0)
	{
		DebugCon_WriteFormatted("RSDP Checksum is not valid '%02hhX', expected '%02hhX'\n", rsdp->Checksum, -(checksum - rsdp->Checksum));
		return; // TODO(MarcasRealAccount): PANIC
	}

	bool useXSDT = false;
	if (rsdp->Revision > 1)
	{
		checksum = ACPIChecksum(rsdp, 36);
		if (checksum == 0)
			useXSDT = true;
		else
			DebugCon_WriteFormatted("RSDP Extended Checksum is not valid '%02hhX', expected '%02hhX' falling back to RSDT\n", rsdp->ExtendedChecksum, -(checksum - rsdp->ExtendedChecksum));
	}

	void*     rootTable   = nullptr;
	uint32_t  entryCount  = 0;
	uint32_t* rsdtEntries = nullptr;
	uint64_t* xsdtEntries = nullptr;
	if (useXSDT)
	{
		struct ACPI_XSDT* xsdt = (struct ACPI_XSDT*) rsdp->XsdtAddress;
		if (*(uint32_t*) &xsdt->Header.Signature != 'TDSX')
		{
			DebugCon_WriteFormatted("XSDT contains invalid signature '%.4s', expected 'XSDT'\n", (const char*) xsdt->Header.Signature);
			return; // TODO(MarcasRealAccount): PANIC
		}
		checksum = ACPIChecksum(xsdt, xsdt->Header.Length);
		if (checksum != 0)
		{
			DebugCon_WriteFormatted("XSDT checksum is not valid '%02hhX', expected '%02hhX'\n", xsdt->Header.Checksum, -(checksum - xsdt->Header.Checksum));
			return; // TODO(MarcasRealAccount): PANIC
		}

		rootTable   = xsdt;
		entryCount  = (xsdt->Header.Length - 36) / 8;
		xsdtEntries = xsdt->Entries;
	}
	else
	{
		struct ACPI_RSDT* rsdt = (struct ACPI_RSDT*) (uint64_t) rsdp->RsdtAddress;
		if (*(uint32_t*) &rsdt->Header.Signature != 'TDSR')
		{
			DebugCon_WriteFormatted("XSDT contains invalid signature '%.4s', expected 'RSDT'\n", (const char*) rsdt->Header.Signature);
			return; // TODO(MarcasRealAccount): PANIC
		}
		checksum = ACPIChecksum(rsdt, rsdt->Header.Length);
		if (checksum != 0)
		{
			DebugCon_WriteFormatted("RSDT checksum is not valid '%02hhX', expected '%02hhX'\n", rsdt->Header.Checksum, -(checksum - rsdt->Header.Checksum));
			return; // TODO(MarcasRealAccount): PANIC
		}

		rootTable   = rsdt;
		entryCount  = (rsdt->Header.Length - 36) / 4;
		rsdtEntries = rsdt->Entries;
	}

	DebugCon_WriteFormatted("RSDP Address: 0x%016lX\n", (uint64_t) rsdp);
	if (useXSDT)
		DebugCon_WriteFormatted("XSDT Address: 0x%016lX, Length: %u, Entry Count: %u\n", (uint64_t) rootTable, ((struct ACPI_DESC_HEADER*) rootTable)->Length, entryCount);
	else
		DebugCon_WriteFormatted("RSDT Address: 0x%016lX, Length: %u, Entry Count: %u\n", (uint64_t) rootTable, ((struct ACPI_DESC_HEADER*) rootTable)->Length, entryCount);

	for (uint32_t i = 0; i < entryCount; ++i)
	{
		struct ACPI_DESC_HEADER* entry = (struct ACPI_DESC_HEADER*) (useXSDT ? xsdtEntries[i] : (uint64_t) rsdtEntries[i]);
		checksum                       = ACPIChecksum(entry, entry->Length);
		if (checksum != 0)
		{
			DebugCon_WriteFormatted("Entry %u, Address: 0x%016lX, Signature: '%.4s' has invalid checksum '%02hhX', expected '%02hhX' skipping\n", i, (uint64_t) entry, (const char*) entry->Signature, entry->Checksum, -(checksum - entry->Checksum));
			continue;
		}

		switch (*(uint32_t*) &entry->Signature)
		{
		case 'PCAF':
		{
			struct ACPI_FADT* fadt = (struct ACPI_FADT*) entry;
			struct ACPI_FACS* facs = (struct ACPI_FACS*) (fadt->XFirmwareControl ? fadt->XFirmwareControl : (uint64_t) fadt->FirmwareControl);
			struct ACPI_DSDT* dsdt = (struct ACPI_DSDT*) (fadt->XDSDT ? fadt->XDSDT : (uint64_t) fadt->DSDT);

			DebugCon_WriteFormatted("  Entry %u, Address: 0x%016lX, Signature: 'FACP' Fixed ACPI Description Table v%hhu.%hhu:\n", i, (uint64_t) entry, fadt->Header.Revision, fadt->MinorVersion);
			DebugCon_WriteString("    Flags: ");
			if (fadt->Flags & ACPI_FADT_WBINVD_MASK)
				DebugCon_WriteString("WBINVD, ");
			if (fadt->Flags & ACPI_FADT_WBINVD_FLUSH_MASK)
				DebugCon_WriteString("WBINVD_FLUSH, ");
			if (fadt->Flags & ACPI_FADT_PROC_C1_MASK)
				DebugCon_WriteString("PROC_C1, ");
			if (fadt->Flags & ACPI_FADT_PLVL2_UP_MASK)
				DebugCon_WriteString("PLVL2_UP, ");
			if (fadt->Flags & ACPI_FADT_POWER_BUTTON_MASK)
				DebugCon_WriteString("POWER_BUTTON, ");
			if (fadt->Flags & ACPI_FADT_SLEEP_BUTTON_MASK)
				DebugCon_WriteString("SLEEP_BUTTON, ");
			if (fadt->Flags & ACPI_FADT_FIX_RTC_MASK)
				DebugCon_WriteString("FIX_RTC, ");
			if (fadt->Flags & ACPI_FADT_RTC_S4_MASK)
				DebugCon_WriteString("RTC_S4, ");
			if (fadt->Flags & ACPI_FADT_TIMER_VAL_EXTENDED_MASK)
				DebugCon_WriteString("TIMER_VAL_EXTENDED, ");
			if (fadt->Flags & ACPI_FADT_DOCK_CAPABLE_MASK)
				DebugCon_WriteString("DOCK_CAPABLE, ");
			if (fadt->Flags & ACPI_FADT_RESET_REG_SUPPORTED_MASK)
				DebugCon_WriteString("RESET_REG_SUPPORTED, ");
			if (fadt->Flags & ACPI_FADT_SEALED_CASE_MASK)
				DebugCon_WriteString("SEALED_CASE, ");
			if (fadt->Flags & ACPI_FADT_HEADLESS_MASK)
				DebugCon_WriteString("HEADLESS, ");
			if (fadt->Flags & ACPI_FADT_CPU_SW_SLEEP_MASK)
				DebugCon_WriteString("CPU_SW_SLEEP, ");
			if (fadt->Flags & ACPI_FADT_PCI_EXP_WAK_MASK)
				DebugCon_WriteString("PCI_EXP_WAK, ");
			if (fadt->Flags & ACPI_FADT_USER_PLATFORM_CLOCK_MASK)
				DebugCon_WriteString("USER_PLATFORM_CLOCK, ");
			if (fadt->Flags & ACPI_FADT_S4_RTC_STS_VALID_MASK)
				DebugCon_WriteString("S4_RTC_STS_VALID, ");
			if (fadt->Flags & ACPI_FADT_REMOTE_POWER_ON_CAPABLE_MASK)
				DebugCon_WriteString("REMOTE_POWER_ON_CAPABLE, ");
			if (fadt->Flags & ACPI_FADT_FORCE_APIC_CLUSTER_MODEL_MASK)
				DebugCon_WriteString("FORCE_APIC_CLUSTER_MODEL, ");
			if (fadt->Flags & ACPI_FADT_FORCE_APIC_PHYSICAL_DESTINATION_MODE_MASK)
				DebugCon_WriteString("FORCE_APIC_PHYSICAL_DESTINATION_MODE, ");
			if (fadt->Flags & ACPI_FADT_HW_REDUCED_ACPI_MASK)
				DebugCon_WriteString("HW_REDUCED_ACPI, ");
			if (fadt->Flags & ACPI_FADT_LOW_POWER_S0_IDLE_CAPABLE_MASK)
				DebugCon_WriteString("LOW_POWER_S0_IDLE_CAPABLE, ");
			if (fadt->Flags & ACPI_FADT_PERSISTENT_CPU_CACHES_MASK)
				DebugCon_WriteString("PERSISTENT_CPU_CACHES, ");
			DebugCon_WriteChar('\n');

			switch (fadt->PreferredPMProfile)
			{
			case 0: DebugCon_WriteString("    Preferred Power Manager Profile Unspecified\n"); break;
			case 1: DebugCon_WriteString("    Preferred Power Manager Profile Desktop\n"); break;
			case 2: DebugCon_WriteString("    Preferred Power Manager Profile Mobile\n"); break;
			case 3: DebugCon_WriteString("    Preferred Power Manager Profile Workstation\n"); break;
			case 4: DebugCon_WriteString("    Preferred Power Manager Profile Enterprise Server\n"); break;
			case 5: DebugCon_WriteString("    Preferred Power Manager Profile SOHO Server\n"); break;
			case 6: DebugCon_WriteString("    Preferred Power Manager Profile Appliance PC\n"); break;
			case 7: DebugCon_WriteString("    Preferred Power Manager Profile Performance Server\n"); break;
			case 8: DebugCon_WriteString("    Preferred Power Manager Profile Tablet\n"); break;
			default: DebugCon_WriteString("    Preferred Power Manager Profile Reserved\n"); break;
			}

			DebugCon_WriteFormatted("    SCI Interrupt: 0x%04hX\n", fadt->SCIInt);
			DebugCon_WriteFormatted("    SMI Command Port: 0x%04X\n", fadt->SMICmd);
			if (facs->Flags & ACPI_FACS_S4BIOS_F_MASK)
				DebugCon_WriteFormatted("      S4BIOS Request: %02hhX\n", fadt->S4BIOSReq);
			if (fadt->PStateControl)
				DebugCon_WriteFormatted("      PState Control: %02hhX\n", fadt->PStateControl);
			if (fadt->CSTControl)
				DebugCon_WriteFormatted("      CST Control: %02hhX\n", fadt->CSTControl);
			DebugCon_WriteFormatted("    PM1 Event Block a Address: 0x%08X, b Address: 0x%08X, Length: %hhu\n", fadt->PM1aEventBlock, fadt->PM1bEventBlock, fadt->PM1EventLength);
			DebugCon_WriteFormatted("    PM1 Control Block a Address: 0x%08X, b Address: 0x%08X, Length: %hhu\n", fadt->PM1aControlBlock, fadt->PM1bControlBlock, fadt->PM1ControlLength);
			DebugCon_WriteFormatted("    PM2 Control Block Address: 0x%08X, Length: %hhu\n", fadt->PM2ControlBlock, fadt->PM2ControlLength);
			DebugCon_WriteFormatted("    PM Timer Block Address: 0x%08X, Length: %hhu\n", fadt->PMTimerBlock, fadt->PMTimerLength);
			DebugCon_WriteFormatted("    GP Event Block 1 Address: 0x%08X, Length: %hhu, 2 Address: 0x%08X, Base: %hu, Length: %hhu\n", fadt->GPE0Block, fadt->GPE0BlockLength, fadt->GPE1Block, fadt->GPE1Base, fadt->GPE1BlockLength);
			if (fadt->PLVL2Latency > 100)
				DebugCon_WriteFormatted("    P LVL2 Unsupported\n");
			else
				DebugCon_WriteFormatted("    P LVL2 Latency: %hu\n", fadt->PLVL2Latency);
			if (fadt->PLVL3Latency > 1000)
				DebugCon_WriteFormatted("    P LVL3 Unsupported\n");
			else
				DebugCon_WriteFormatted("    P LVL3 Latency: %hu\n", fadt->PLVL3Latency);
			if (fadt->FlushSize == 0 && !(fadt->Flags & ACPI_FADT_WBINVD_MASK))
				DebugCon_WriteFormatted("    Cache flush Unsupported\n");
			else
				DebugCon_WriteFormatted("    Cache flush Size: %hu, Stride: %hu\n", fadt->FlushSize, fadt->FlushStride);
			DebugCon_WriteFormatted("    Duty Offset: %hhu, Width %hhu\n", fadt->DutyOffset, fadt->DutyWidth);
			if (fadt->DayAlarm == 0 && fadt->MonthAlarm == 0 && fadt->Century == 0)
			{
				DebugCon_WriteFormatted("    RTC CMOS Unsupported\n");
			}
			else
			{
				DebugCon_WriteString("    RTC CMOS");
				if (fadt->DayAlarm != 0)
					DebugCon_WriteFormatted(", Day Alarm index: %hhu", fadt->DayAlarm);
				if (fadt->MonthAlarm != 0)
					DebugCon_WriteFormatted(", Month Alarm index: %hhu", fadt->MonthAlarm);
				if (fadt->Century != 0)
					DebugCon_WriteFormatted(", Century index: %hhu", fadt->Century);
				DebugCon_WriteChar('\n');
			}

			if (*(uint64_t*) fadt->HypervisorVendorIdentity != 0)
				DebugCon_WriteFormatted("    Hypervisor Vendor Identity: %016lX\n", *(uint64_t*) fadt->HypervisorVendorIdentity);

			DebugCon_WriteFormatted("    FACS Address: 0x%016lX, Signature: '%.4s', Hardware Signature '%.4s', Version %hhu:\n", (uint64_t) facs, (const char*) facs->Signature, (const char*) facs->HardwareSignature, facs->Version);

			DebugCon_WriteString("      Flags: ");
			if (facs->Flags & ACPI_FACS_S4BIOS_F_MASK)
				DebugCon_WriteString("S4BIOS_F, ");
			if (facs->Flags & ACPI_FACS_64BIT_WAKE_SUPPORTED_MASK)
				DebugCon_WriteString("64BIT_WAKE_SUPPORTED_F, ");
			DebugCon_WriteChar('\n');

			checksum = ACPIChecksum(dsdt, dsdt->Header.Length);
			if (checksum == 0)
				DebugCon_WriteFormatted("    DSDT Address: 0x%016lX, Length: %u\n", (uint64_t) dsdt, dsdt->Header.Length - 36);
			else
				DebugCon_WriteFormatted("    DSDT Address: 0x%016lX has invalid checksum '%02hhX', expected '%02hhX' skipping\n", (uint64_t) dsdt, entry->Checksum, -(checksum - entry->Checksum));
			break;
		}
		case 'CIPA':
		{
			struct ACPI_MADT* madt = (struct ACPI_MADT*) entry;

			DebugCon_WriteFormatted("  Entry %u, Address: 0x%016lX, Signature: 'APIC' Multiple APIC Description Table:\n", i, (uint64_t) entry);

			DebugCon_WriteString("    Flags: ");
			if (madt->Flags & ACPI_MADT_PCAT_COMPAT_MASK)
				DebugCon_WriteString("PCAT_COMPAT, ");
			DebugCon_WriteChar('\n');

			DebugCon_WriteFormatted("    Local Interrupt Controller Address: 0x%08X\n", madt->LICA);
			uint32_t                    icEntry = 0;
			size_t                      offset  = 44;
			struct ACPI_MADT_IC_HEADER* curIC   = madt->Entries;
			while (offset < madt->Header.Length)
			{
				uint8_t len = curIC->Length;
				switch (curIC->Type)
				{
				case ACPI_MADT_IC_PROCESSOR_LAPIC: DebugCon_WriteFormatted("      Entry %u, Type: 'Processor Local APIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_IOAPIC: DebugCon_WriteFormatted("      Entry %u, Type: 'IO APIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_ISO: DebugCon_WriteFormatted("      Entry %u, Type: 'ISO', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_NMIS: DebugCon_WriteFormatted("      Entry %u, Type: 'NMI Source', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_LAPIC_NMI: DebugCon_WriteFormatted("      Entry %u, Type: 'Local APIC NMI', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_LAPIC_ADDRESS_OVERRIDE: DebugCon_WriteFormatted("      Entry %u, Type: 'Local APIC Address Override', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_IO_SAPIC: DebugCon_WriteFormatted("      Entry %u, Type: 'IO SAPIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_LSAPIC: DebugCon_WriteFormatted("      Entry %u, Type: 'Local SAPIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_PIS: DebugCon_WriteFormatted("      Entry %u, Type: 'Platform Interrupt Sources', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_PROCESSOR_Lx2APIC: DebugCon_WriteFormatted("      Entry %u, Type: 'Processor Local x2APIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_Lx2APIC_NMI: DebugCon_WriteFormatted("      Entry %u, Type: 'Local x2APIC NMI', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_GICC: DebugCon_WriteFormatted("      Entry %u, Type: 'GIC CPU Interface', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_GICD: DebugCon_WriteFormatted("      Entry %u, Type: 'GIC Distributor', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_GIC_MSI_FRAME: DebugCon_WriteFormatted("      Entry %u, Type: 'GIC MSI Frame', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_GICR: DebugCon_WriteFormatted("      Entry %u, Type: 'GIC Redistributor', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_GIC_ITS: DebugCon_WriteFormatted("      Entry %u, Type: 'GIC Interrupt Translation Service', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_MULTIPROCESSOR_WAKEUP: DebugCon_WriteFormatted("      Entry %u, Type: 'Multiprocessor Wakeup', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_CORE_PIC: DebugCon_WriteFormatted("      Entry %u, Type: 'CORE PIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_LIO_PIC: DebugCon_WriteFormatted("      Entry %u, Type: 'LIO PIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_HT_PIC: DebugCon_WriteFormatted("      Entry %u, Type: 'HT PIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_EIO_PIC: DebugCon_WriteFormatted("      Entry %u, Type: 'EIO PIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_MSI_PIC: DebugCon_WriteFormatted("      Entry %u, Type: 'MSI PIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_BIO_PIC: DebugCon_WriteFormatted("      Entry %u, Type: 'BIO PIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				case ACPI_MADT_IC_LPC_PIC: DebugCon_WriteFormatted("      Entry %u, Type: 'LPC PIC', Length: %hhu:\n", icEntry, curIC->Length); break;
				default: DebugCon_WriteFormatted("      Entry %u, Type: %02hhX, Length: %hhu Unknown\n", icEntry, curIC->Type, curIC->Length); break;
				}
				curIC   = (struct ACPI_MADT_IC_HEADER*) ((uint8_t*) curIC + len);
				offset += len;
				++icEntry;
			}
			break;
		}
		default:
			DebugCon_WriteFormatted("  Entry %u, Address: 0x%016lX, Signature: '%.4s' unused skipping\n", i, (uint64_t) entry, (const char*) entry->Signature);
			break;
		}
	}
}