#pragma once

#include <stddef.h>
#include <stdint.h>

enum FramebufferFormat
{
	FramebufferFormatARGB8,
	FramebufferFormatRGBA8,
	FramebufferFormatRGB8,
	FramebufferFormatBGR8
};

enum FramebufferColorspace
{
	FramebufferColorspaceLinear,
	FramebufferColorspaceSRGB
};

struct Framebuffer
{
	size_t Width, Height, Pitch;
	void*  Content;

	enum FramebufferFormat     Format;
	enum FramebufferColorspace Colorspace;
};

struct LinearColor
{
	uint16_t r, g, b, a;
};

struct GraphicsRect
{
	size_t x, y;
	size_t w, h;
};

struct GraphicsPoint
{
	size_t x, y;
};

struct FontHeader
{
	uint8_t  CharWidth;
	uint8_t  CharHeight;
	uint8_t  Bitdepth;
	uint8_t  Pad0;
	uint32_t Pad1;
	uint64_t CharacterCount;
};

struct FontCharacterLUT
{
	uint32_t Character;
	uint8_t  Width;
	uint8_t  Pad[3];
	size_t   Offset;
};

void LoadFont(struct FontHeader* font);

void GraphicsDrawRect(struct Framebuffer* framebuffer, struct GraphicsRect rect, struct LinearColor fillColor, struct LinearColor outlineColor);
void GraphicsDrawLine(struct Framebuffer* framebuffer, struct GraphicsPoint a, struct GraphicsPoint b, struct LinearColor color);
void GraphicsDrawText(struct Framebuffer* framebuffer, struct GraphicsPoint pos, const char* text, size_t count, struct LinearColor color);