#pragma once

#include <stdint.h>

struct InterruptHandlerData
{
	uint64_t returnRIP;
	uint64_t returnCS;
	uint64_t returnRFlags;
	uint64_t returnRSP;
	uint64_t returnSS;
};

struct TrapHandlerData
{
	uint64_t errorCode;
	uint64_t returnRIP;
	uint64_t returnCS;
	uint64_t returnRFlags;
	uint64_t returnRSP;
	uint64_t returnSS;
};

struct IDTEntry
{
	uint64_t low, high;
};

extern uint64_t        g_GDT[512];
extern struct IDTEntry g_IDT[256];

void LoadDescriptorTables(void);

bool ClearSegment(uint16_t segment);
bool SetCodeSegment(uint16_t segment, uint8_t privilege, bool conforming);
bool SetDataSegment(uint16_t segment);
void SetInterruptHandler(uint8_t vector, uint16_t selector, uint8_t ist, uint8_t privilege, void (*handler)(struct InterruptHandlerData* data));
void SetTrapHandler(uint8_t vector, uint16_t selector, uint8_t ist, uint8_t privilege, void (*handler)(struct TrapHandlerData* data));