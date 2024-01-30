#include "IO/Debug.h"
#include "Arches/x86_64/IO.h"

#include <string.h>

void DebugOutChar(char c)
{
	RawOut8(0xE9, (uint8_t) c);
}

void DebugOutChars(const char* restrict str, size_t size)
{
	RawOutBytes(0xE9, str, size);
}

void DebugOutString(const char* restrict str)
{
	DebugOutChars(str, strlen(str));
}