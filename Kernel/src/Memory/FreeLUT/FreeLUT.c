#include "Memory/FreeLUT/FreeLUT.h"

uint64_t FreeLUTGetValue(uint8_t index)
{
	if (index < 192)
		return (uint64_t) index + 1;
	return (1UL << (index - 192)) + 192;
}

uint8_t FreeLUTGetFloorIndex(uint64_t value)
{
	if (value < 193)
		return (uint8_t) value - 1;
	return (254 - __builtin_clzll(value - 192));
}

uint8_t FreeLUTGetCeilIndex(uint64_t value)
{
	if (value < 193)
		return (uint8_t) value - 1;
	return (255 - __builtin_clzll(value - 193));
}