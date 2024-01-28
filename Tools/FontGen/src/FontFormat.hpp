#pragma once

#include <cstdint>

struct FontFileCharacterLUT
{
	uint32_t Character;
	uint8_t  Width;
	uint8_t  Pad0;
	uint16_t PaddedWidth;
	uint64_t Offset;
};

struct FontFileHeader
{
	uint8_t              CharWidth;
	uint8_t              CharHeight;
	uint8_t              BitDepth;
	uint8_t              Pad0;
	uint32_t             Pad1;
	uint64_t             CharacterCount;
	FontFileCharacterLUT LUT[1];
};