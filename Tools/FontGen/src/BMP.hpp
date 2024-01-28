#pragma once

#include <cstdint>

struct BMPHeader
{
	uint32_t Size;
	uint32_t Reserved;
	uint32_t Offset;

	uint32_t HeaderSize;
	uint32_t Width;
	uint32_t Height;
	uint16_t Planes;
	uint16_t Bpp;
	uint32_t Compression;
	uint32_t ImageSize;
	uint32_t ResX;
	uint32_t ResY;
	uint32_t ColorPallete;
	uint32_t ImportantColors;
};