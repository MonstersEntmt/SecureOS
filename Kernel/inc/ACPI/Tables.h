#pragma once

#include <stdint.h>

struct ACPI_GAS
{
	uint8_t  AddressSpaceID;
	uint8_t  RegisterBitWidth;
	uint8_t  RegisterBitOffset;
	uint8_t  AddressSize;
	uint64_t Address;
} __attribute__((packed));

struct ACPI_RSDP
{
	uint8_t  Signature[8];
	uint8_t  Checksum;
	uint8_t  OEMID[6];
	uint8_t  Revision;
	uint32_t RsdtAddress;

	uint32_t Length;
	uint64_t XsdtAddress;
	uint8_t  ExtendedChecksum;
	uint8_t  Reserved[3];
} __attribute__((packed));

struct ACPI_DESC_HEADER
{
	uint8_t  Signature[4];
	uint32_t Length;
	uint8_t  Revision;
	uint8_t  Checksum;
	uint8_t  OEMID[6];
	uint64_t OEMTableID;
	uint32_t OEMRevision;
	uint32_t CreatorID;
	uint32_t CreatorRevision;
} __attribute__((packed));

struct ACPI_RSDT
{
	struct ACPI_DESC_HEADER Header;
	uint32_t                Entries[1];
} __attribute__((packed));

struct ACPI_XSDT
{
	struct ACPI_DESC_HEADER Header;
	uint64_t                Entries[1];
} __attribute__((packed));

struct ACPI_FADT
{
	struct ACPI_DESC_HEADER Header;
	uint32_t                FirmwareControl;
	uint32_t                DSDT;
	uint8_t                 Reserved;
	uint8_t                 PreferredPMProfile;
	uint16_t                SCIInt;
	uint32_t                SMICmd;
	uint8_t                 ACPIEnable;
	uint8_t                 ACPIDisable;
	uint8_t                 S4BIOSReq;
	uint8_t                 PStateControl;
	uint32_t                PM1aEventBlock;
	uint32_t                PM1bEventBlock;
	uint32_t                PM1aControlBlock;
	uint32_t                PM1bControlBlock;
	uint32_t                PM2ControlBlock;
	uint32_t                PMTimerBlock;
	uint32_t                GPE0Block;
	uint32_t                GPE1Block;
	uint8_t                 PM1EventLength;
	uint8_t                 PM1ControlLength;
	uint8_t                 PM2ControlLength;
	uint8_t                 PMTimerLength;
	uint8_t                 GPE0BlockLength;
	uint8_t                 GPE1BlockLength;
	uint8_t                 GPE1Base;
	uint8_t                 CSTControl;
	uint16_t                PLVL2Latency;
	uint16_t                PLVL3Latency;
	uint16_t                FlushSize;
	uint16_t                FlushStride;
	uint8_t                 DutyOffset;
	uint8_t                 DutyWidth;
	uint8_t                 DayAlarm;
	uint8_t                 MonthAlarm;
	uint8_t                 Century;
	uint16_t                IAPCBootArch;
	uint8_t                 Reserved2;
	uint32_t                Flags;
	struct ACPI_GAS         ResetReg;
	uint8_t                 ResetValue;
	uint16_t                ArmBootArch;
	uint8_t                 MinorVersion;
	uint64_t                XFirmwareControl;
	uint64_t                XDSDT;
	struct ACPI_GAS         XPM1aEventBlock;
	struct ACPI_GAS         XPM1bEventBlock;
	struct ACPI_GAS         XPM1aControlBlock;
	struct ACPI_GAS         XPM1bControlBlock;
	struct ACPI_GAS         XPM2ControlBlock;
	struct ACPI_GAS         XPMTimerBlock;
	struct ACPI_GAS         XGPE0Block;
	struct ACPI_GAS         XGPE1Block;
	struct ACPI_GAS         SleepControlReg;
	struct ACPI_GAS         SleepStatusReg;
	uint8_t                 HypervisorVendorIdentity[8];
} __attribute__((packed));

struct ACPI_FACS
{
	uint8_t  Signature[4];
	uint32_t Length;
	uint8_t  HardwareSignature[4];
	uint32_t FirmwareWakingVector;
	uint32_t GlobalLock;
	uint32_t Flags;
	uint64_t XFirwmareWakingVector;
	uint8_t  Version;
	uint8_t  Reserved[3];
	uint32_t OSPMFlags;
	uint8_t  Reserved2[24];
} __attribute__((packed));

struct ACPI_DSDT
{
	struct ACPI_DESC_HEADER Header;
	uint8_t                 AMLCode[1];
} __attribute__((packed));

struct ACPI_MADT_IC_HEADER
{
	uint8_t Type;
	uint8_t Length;
} __attribute__((packed));

struct ACPI_MADT
{
	struct ACPI_DESC_HEADER    Header;
	uint32_t                   LICA; // Local Interrupt Controller Address
	uint32_t                   Flags;
	struct ACPI_MADT_IC_HEADER Entries[1];
} __attribute__((packed));

struct ACPI_MADT_LAPIC
{
	struct ACPI_MADT_IC_HEADER Header;
	uint8_t                    ACPIProcessorUID;
	uint8_t                    APICID;
	uint32_t                   Flags;
} __attribute__((packed));

struct ACPI_MADT_IO_APIC
{
	struct ACPI_MADT_IC_HEADER Header;
	uint8_t                    IOAPICID;
	uint8_t                    Reserved;
	uint32_t                   IOAPICAddress;
	uint32_t                   GSIB; // Global System Interrupt Base
} __attribute__((packed));

struct ACPI_MADT_ISO
{
	struct ACPI_MADT_IC_HEADER Header;
	uint8_t                    Bus;
	uint8_t                    Source;
	uint32_t                   GSI; // Global Source Interrupt
	uint16_t                   Flags;
} __attribute__((packed));

struct ACPI_MADT_NMIS
{
	struct ACPI_MADT_IC_HEADER Header;
	uint16_t                   Flags;
	uint32_t                   GSI; // Global System Interrupt
} __attribute((packed));

struct ACPI_MADT_LAPIC_NMIS
{
	struct ACPI_MADT_IC_HEADER Header;
	uint8_t                    ACPIProcessorUID;
	uint16_t                   Flags;
	uint8_t                    LAPIC_LINT;
} __attribute((packed));

struct ACPI_MADT_LAPIC_AO
{
	struct ACPI_MADT_IC_HEADER Header;
	uint16_t                   Reserved;
	uint64_t                   APICAddress;
} __attribute((packed));

struct ACPI_MADT_IO_SAPIC
{
	struct ACPI_MADT_IC_HEADER Header;
	uint8_t                    IOAPICID;
	uint8_t                    Reserved;
	uint32_t                   GSIB; // Global System Interrupt Base
	uint64_t                   IOSAPICAddress;
} __attribute__((packed));

struct ACPI_MADT_LSAPIC
{
	struct ACPI_MADT_IC_HEADER Header;
	uint8_t                    ACPIProcessorID;
	uint8_t                    LSAPICID;
	uint8_t                    LSAPICEID;
	uint8_t                    Reserved[3];
	uint32_t                   Flags;
	uint32_t                   ACPIProcessorUID;
	uint8_t                    ACPIProcessorUIDStr[1];
} __attribute__((packed));

struct ACPI_MADT_PIS
{
	struct ACPI_MADT_IC_HEADER Header;
	uint16_t                   Flags;
	uint8_t                    InterruptType;
	uint8_t                    ProcessorID;
	uint8_t                    ProcessorEID;
	uint8_t                    IOSAPICVector;
	uint32_t                   GSI;      // Global System Interrupt
	uint32_t                   PISFlags; // Platform Interrupt Source Flags
} __attribute__((packed));

struct ACPI_MADT_Lx2APIC
{
	struct ACPI_MADT_IC_HEADER Header;
	uint16_t                   Reserved;
	uint32_t                   X2APICID;
	uint32_t                   Flags;
	uint32_t                   ACPIProcessorUID;
} __attribute__((packed));

struct ACPI_MADT_Lx2APIC_NMIS
{
	struct ACPI_MADT_IC_HEADER Header;
	uint16_t                   Flags;
	uint32_t                   ACPIProcessorUID;
	uint8_t                    Lx2APIC_LINT;
	uint8_t                    Reserved[3];
} __attribute((packed));

#define ACPI_FADT_WBINVD_MASK                               0x0000'0001U
#define ACPI_FADT_WBINVD_FLUSH_MASK                         0x0000'0002U
#define ACPI_FADT_PROC_C1_MASK                              0x0000'0004U
#define ACPI_FADT_PLVL2_UP_MASK                             0x0000'0008U
#define ACPI_FADT_POWER_BUTTON_MASK                         0x0000'0010U
#define ACPI_FADT_SLEEP_BUTTON_MASK                         0x0000'0020U
#define ACPI_FADT_FIX_RTC_MASK                              0x0000'0040U
#define ACPI_FADT_RTC_S4_MASK                               0x0000'0080U
#define ACPI_FADT_TIMER_VAL_EXTENDED_MASK                   0x0000'0100U
#define ACPI_FADT_DOCK_CAPABLE_MASK                         0x0000'0200U
#define ACPI_FADT_RESET_REG_SUPPORTED_MASK                  0x0000'0400U
#define ACPI_FADT_SEALED_CASE_MASK                          0x0000'0800U
#define ACPI_FADT_HEADLESS_MASK                             0x0000'1000U
#define ACPI_FADT_CPU_SW_SLEEP_MASK                         0x0000'2000U
#define ACPI_FADT_PCI_EXP_WAK_MASK                          0x0000'4000U
#define ACPI_FADT_USER_PLATFORM_CLOCK_MASK                  0x0000'8000U
#define ACPI_FADT_S4_RTC_STS_VALID_MASK                     0x0001'0000U
#define ACPI_FADT_REMOTE_POWER_ON_CAPABLE_MASK              0x0002'0000U
#define ACPI_FADT_FORCE_APIC_CLUSTER_MODEL_MASK             0x0004'0000U
#define ACPI_FADT_FORCE_APIC_PHYSICAL_DESTINATION_MODE_MASK 0x0008'0000U
#define ACPI_FADT_HW_REDUCED_ACPI_MASK                      0x0010'0000U
#define ACPI_FADT_LOW_POWER_S0_IDLE_CAPABLE_MASK            0x0020'0000U
#define ACPI_FADT_PERSISTENT_CPU_CACHES_MASK                0x0040'0000U
#define ACPI_FACS_S4BIOS_F_MASK                             0x0000'0001U
#define ACPI_FACS_64BIT_WAKE_SUPPORTED_MASK                 0x0000'0002U
#define ACPI_FACS_OSPM_64BIT_WAKE                           0x0000'0001U
#define ACPI_MADT_PCAT_COMPAT_MASK                          0x0000'0001U
#define ACPI_MADT_IC_PROCESSOR_LAPIC                        0x00
#define ACPI_MADT_IC_IOAPIC                                 0x01
#define ACPI_MADT_IC_ISO                                    0x02
#define ACPI_MADT_IC_NMIS                                   0x03
#define ACPI_MADT_IC_LAPIC_NMI                              0x04
#define ACPI_MADT_IC_LAPIC_ADDRESS_OVERRIDE                 0x05
#define ACPI_MADT_IC_IO_SAPIC                               0x06
#define ACPI_MADT_IC_LSAPIC                                 0x07
#define ACPI_MADT_IC_PIS                                    0x08
#define ACPI_MADT_IC_PROCESSOR_Lx2APIC                      0x09
#define ACPI_MADT_IC_Lx2APIC_NMI                            0x0A
#define ACPI_MADT_IC_GICC                                   0x0B
#define ACPI_MADT_IC_GICD                                   0x0C
#define ACPI_MADT_IC_GIC_MSI_FRAME                          0x0D
#define ACPI_MADT_IC_GICR                                   0x0E
#define ACPI_MADT_IC_GIC_ITS                                0x0F
#define ACPI_MADT_IC_MULTIPROCESSOR_WAKEUP                  0x10
#define ACPI_MADT_IC_CORE_PIC                               0x11
#define ACPI_MADT_IC_LIO_PIC                                0x12
#define ACPI_MADT_IC_HT_PIC                                 0x13
#define ACPI_MADT_IC_EIO_PIC                                0x14
#define ACPI_MADT_IC_MSI_PIC                                0x15
#define ACPI_MADT_IC_BIO_PIC                                0x16
#define ACPI_MADT_IC_LPC_PIC                                0x17
#define ACPI_MADT_LAPIC_ENABLED_MASK                        0x0000'0001U
#define ACPI_MADT_LAPIC_ONLINE_CAPABLE_MASK                 0x0000'0002U
#define ACPI_MADT_ISO_POLARITY_MASK                         0x0000'0003U
#define ACPI_MADT_ISO_TRIGGER_MODE_MASK                     0x0000'000CU
#define ACPI_MADT_PIS_CPEI_PROCESSOR_OVERRIDE_MASK          0x0000'0001U