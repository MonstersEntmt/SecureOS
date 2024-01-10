#pragma once

#include <cstddef>
#include <cstdint>

#include <ostream>
#include <string_view>

struct GUID
{
	uint32_t TimeLow;
	uint16_t TimeMid;
	uint16_t TimeHiAndVersion;
	uint8_t  ClockSeqHiAndReserved;
	uint8_t  ClockSeqLow;
	uint8_t  Node[6];
};

bool GUIDToString(const GUID& guid, char* buf, size_t bufSize);
bool ParseGUID(std::string_view arg, GUID& guid);
GUID RandomGUID();

std::ostream& operator<<(std::ostream& stream, const GUID& guid);