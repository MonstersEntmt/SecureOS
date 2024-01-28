#include "GUID.hpp"
#include "Random.hpp"

#include <cstring>

bool operator==(const GUID& lhs, const GUID& rhs)
{
	return memcmp(&lhs, &rhs, sizeof(GUID)) == 0;
}

bool GUIDToString(const GUID& guid, char* buf, size_t bufSize)
{
	static constexpr const char c_HexDigits[] = "0123456789abcdef";
	if (bufSize < 37)
		return false;
	buf[0]  = c_HexDigits[(guid.TimeLow >> 28) & 0xF];
	buf[1]  = c_HexDigits[(guid.TimeLow >> 24) & 0xF];
	buf[2]  = c_HexDigits[(guid.TimeLow >> 20) & 0xF];
	buf[3]  = c_HexDigits[(guid.TimeLow >> 16) & 0xF];
	buf[4]  = c_HexDigits[(guid.TimeLow >> 12) & 0xF];
	buf[5]  = c_HexDigits[(guid.TimeLow >> 8) & 0xF];
	buf[6]  = c_HexDigits[(guid.TimeLow >> 4) & 0xF];
	buf[7]  = c_HexDigits[guid.TimeLow & 0xF];
	buf[8]  = '-';
	buf[9]  = c_HexDigits[(guid.TimeMid >> 12) & 0xF];
	buf[10] = c_HexDigits[(guid.TimeMid >> 8) & 0xF];
	buf[11] = c_HexDigits[(guid.TimeMid >> 4) & 0xF];
	buf[12] = c_HexDigits[guid.TimeMid & 0xF];
	buf[13] = '-';
	buf[14] = c_HexDigits[(guid.TimeHiAndVersion >> 12) & 0xF];
	buf[15] = c_HexDigits[(guid.TimeHiAndVersion >> 8) & 0xF];
	buf[16] = c_HexDigits[(guid.TimeHiAndVersion >> 4) & 0xF];
	buf[17] = c_HexDigits[guid.TimeHiAndVersion & 0xF];
	buf[18] = '-';
	buf[19] = c_HexDigits[(guid.ClockSeqHiAndReserved >> 4) & 0xF];
	buf[20] = c_HexDigits[guid.ClockSeqHiAndReserved & 0xF];
	buf[21] = c_HexDigits[(guid.ClockSeqLow >> 4) & 0xF];
	buf[22] = c_HexDigits[guid.ClockSeqLow & 0xF];
	buf[23] = '-';
	buf[24] = c_HexDigits[(guid.Node[0] >> 4) & 0xF];
	buf[25] = c_HexDigits[guid.Node[0] & 0xF];
	buf[26] = c_HexDigits[(guid.Node[1] >> 4) & 0xF];
	buf[27] = c_HexDigits[guid.Node[1] & 0xF];
	buf[28] = c_HexDigits[(guid.Node[2] >> 4) & 0xF];
	buf[29] = c_HexDigits[guid.Node[2] & 0xF];
	buf[30] = c_HexDigits[(guid.Node[3] >> 4) & 0xF];
	buf[31] = c_HexDigits[guid.Node[3] & 0xF];
	buf[32] = c_HexDigits[(guid.Node[4] >> 4) & 0xF];
	buf[33] = c_HexDigits[guid.Node[4] & 0xF];
	buf[34] = c_HexDigits[(guid.Node[5] >> 4) & 0xF];
	buf[35] = c_HexDigits[guid.Node[5] & 0xF];
	buf[36] = '\0';
	return true;
}

static uint8_t ParseHexDigit(char c)
{
	switch (c)
	{
	case '0': return 0x0;
	case '1': return 0x1;
	case '2': return 0x2;
	case '3': return 0x3;
	case '4': return 0x4;
	case '5': return 0x5;
	case '6': return 0x6;
	case '7': return 0x7;
	case '8': return 0x8;
	case '9': return 0x9;
	case 'a':
	case 'A': return 0xA;
	case 'b':
	case 'B': return 0xB;
	case 'c':
	case 'C': return 0xC;
	case 'd':
	case 'D': return 0xD;
	case 'e':
	case 'E': return 0xE;
	case 'f':
	case 'F': return 0xF;
	default: return 0xFF;
	}
}

bool ParseGUID(std::string_view arg, GUID& guid)
{
	if (arg.size() < 36 || arg[8] != '-' || arg[13] != '-' || arg[18] != '-' || arg[23] != '-')
		return false;

	guid.TimeLow               = ParseHexDigit(arg[0]) << 28 | ParseHexDigit(arg[1]) << 24 | ParseHexDigit(arg[2]) << 20 | ParseHexDigit(arg[3]) << 16 | ParseHexDigit(arg[4]) << 12 | ParseHexDigit(arg[5]) << 8 | ParseHexDigit(arg[6]) << 4 | ParseHexDigit(arg[7]);
	guid.TimeMid               = ParseHexDigit(arg[9]) << 12 | ParseHexDigit(arg[10]) << 8 | ParseHexDigit(arg[11]) << 4 | ParseHexDigit(arg[12]);
	guid.TimeHiAndVersion      = ParseHexDigit(arg[14]) << 12 | ParseHexDigit(arg[15]) << 8 | ParseHexDigit(arg[16]) << 4 | ParseHexDigit(arg[17]);
	guid.ClockSeqHiAndReserved = ParseHexDigit(arg[19]) << 4 | ParseHexDigit(arg[20]);
	guid.ClockSeqLow           = ParseHexDigit(arg[21]) << 4 | ParseHexDigit(arg[22]);
	guid.Node[0]               = ParseHexDigit(arg[24]) << 4 | ParseHexDigit(arg[25]);
	guid.Node[1]               = ParseHexDigit(arg[26]) << 4 | ParseHexDigit(arg[27]);
	guid.Node[2]               = ParseHexDigit(arg[28]) << 4 | ParseHexDigit(arg[29]);
	guid.Node[3]               = ParseHexDigit(arg[30]) << 4 | ParseHexDigit(arg[31]);
	guid.Node[4]               = ParseHexDigit(arg[32]) << 4 | ParseHexDigit(arg[33]);
	guid.Node[5]               = ParseHexDigit(arg[34]) << 4 | ParseHexDigit(arg[35]);
	return true;
}

GUID RandomGUID()
{
	GUID      guid {};
	uint64_t* pU64             = (uint64_t*) &guid;
	pU64[0]                    = RandomU64();
	pU64[1]                    = RandomU64();
	guid.ClockSeqHiAndReserved = guid.ClockSeqHiAndReserved & 0x3F | 0x80;
	guid.TimeHiAndVersion      = guid.TimeHiAndVersion & 0xF | 0x40;
	return guid;
}

std::ostream& operator<<(std::ostream& stream, const GUID& guid)
{
	char buf[37];
	GUIDToString(guid, buf, 37);
	stream << buf;
	return stream;
}