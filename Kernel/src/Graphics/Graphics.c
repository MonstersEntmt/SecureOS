#include "Graphics/Graphics.h"
#include "DebugCon.h"
#include "KernelVMM.h"
#include "PMM.h"
#include "VMM.h"

#include <stdint.h>
#include <string.h>

struct FontCharacter
{
	uint8_t Width;
	void*   BitmapAddress;
};

uint8_t               g_FontWidth;
uint8_t               g_FontHeight;
struct FontCharacter* g_FontCharacters;

void LoadFont(struct FontHeader* font)
{
	if (!font)
		return;


	void* kernelPagetable = GetKernelPageTable();
	if (!g_FontCharacters)
	{
		g_FontCharacters = (struct FontCharacter*) VMMAlloc(kernelPagetable, 4352, 0, VMM_PAGE_TYPE_4KIB, VMM_PAGE_PROTECT_READ_WRITE);
		g_FontWidth      = font->CharWidth;
		g_FontHeight     = font->CharHeight;
	}
	if (font->CharWidth != g_FontWidth || font->CharHeight != g_FontHeight || font->Bitdepth != 1)
		return;
	struct FontCharacterLUT* fontCharacters = (struct FontCharacterLUT*) (font + 1);
	for (size_t i = 0; i < font->CharacterCount; ++i)
	{
		struct FontCharacterLUT* fontCharacter   = &fontCharacters[i];
		void*                    physicalAddress = VMMTranslate(kernelPagetable, &g_FontCharacters[fontCharacter->Character]);
		if (!physicalAddress)
		{
			physicalAddress = PMMAlloc(1);
			if (!physicalAddress)
			{
				DebugCon_WriteFormatted("Failed to allocate page for font\n");
				break;
			}
			memset(physicalAddress, 0, 4096);
			VMMMap(kernelPagetable, &g_FontCharacters[fontCharacter->Character], physicalAddress);
		}
		g_FontCharacters[fontCharacter->Character] = (struct FontCharacter) {
			.Width         = fontCharacter->Width,
			.BitmapAddress = (uint8_t*) font + fontCharacter->Offset
		};
	}
}

static struct LinearColor GraphicsLinearToColorspace(struct LinearColor color, enum FramebufferColorspace colorspace)
{
	switch (colorspace)
	{
	case FramebufferColorspaceLinear: return color;
	case FramebufferColorspaceSRGB:
	default: return color;
	}
}

static void GraphicsSetPixel(struct Framebuffer* framebuffer, size_t x, size_t y, struct LinearColor color)
{
	switch (framebuffer->Format)
	{
	case FramebufferFormatARGB8:
	{
		size_t   index       = x * 4 + framebuffer->Pitch * y;
		uint8_t* subpixels   = (uint8_t*) framebuffer->Content;
		subpixels[index]     = color.b >> 8;
		subpixels[index + 1] = color.g >> 8;
		subpixels[index + 2] = color.r >> 8;
		subpixels[index + 3] = color.a >> 8;
		break;
	}
	case FramebufferFormatRGBA8:
	{
		size_t   index       = x * 4 + framebuffer->Pitch * y;
		uint8_t* subpixels   = (uint8_t*) framebuffer->Content;
		subpixels[index]     = color.a >> 8;
		subpixels[index + 1] = color.b >> 8;
		subpixels[index + 2] = color.g >> 8;
		subpixels[index + 3] = color.r >> 8;
		break;
	}
	case FramebufferFormatRGB8:
	{
		size_t   index       = x * 3 + framebuffer->Pitch * y;
		uint8_t* subpixels   = (uint8_t*) framebuffer->Content;
		subpixels[index]     = color.b >> 8;
		subpixels[index + 1] = color.g >> 8;
		subpixels[index + 2] = color.r >> 8;
		break;
	}
	case FramebufferFormatBGR8:
	{
		size_t   index       = x * 3 + framebuffer->Pitch * y;
		uint8_t* subpixels   = (uint8_t*) framebuffer->Content;
		subpixels[index]     = color.r >> 8;
		subpixels[index + 1] = color.g >> 8;
		subpixels[index + 2] = color.b >> 8;
		break;
	}
	}
}

static void GraphicsDrawOutlinedRect(struct Framebuffer* framebuffer, struct GraphicsRect rect, struct LinearColor fillColor, struct LinearColor outlineColor)
{
	size_t sx = framebuffer->Width < rect.x ? framebuffer->Width : rect.x;
	size_t sy = framebuffer->Height < rect.y ? framebuffer->Height : rect.y;
	size_t ex = framebuffer->Width < rect.x + rect.w ? framebuffer->Width : rect.x + rect.w;
	size_t ey = framebuffer->Height < rect.y + rect.h ? framebuffer->Height : rect.y + rect.h;

	struct LinearColor fillColorspaceAdjusted    = GraphicsLinearToColorspace(fillColor, framebuffer->Colorspace);
	struct LinearColor outlineColorspaceAdjusted = GraphicsLinearToColorspace(outlineColor, framebuffer->Colorspace);
	for (size_t y = sy; y < ey; ++y)
	{
		for (size_t x = sx; x < ex; ++x)
			GraphicsSetPixel(framebuffer, x, y, x == sx || y == sy || x == (ex - 1) || y == (ey - 1) ? outlineColorspaceAdjusted : fillColorspaceAdjusted);
	}
}

void GraphicsDrawRect(struct Framebuffer* framebuffer, struct GraphicsRect rect, struct LinearColor fillColor, struct LinearColor outlineColor)
{
	if (!framebuffer)
		return;

	if (fillColor.r != outlineColor.r ||
		fillColor.g != outlineColor.g ||
		fillColor.b != outlineColor.b ||
		fillColor.a != outlineColor.a)
		return GraphicsDrawOutlinedRect(framebuffer, rect, fillColor, outlineColor);

	size_t sx = framebuffer->Width < rect.x ? framebuffer->Width : rect.x;
	size_t sy = framebuffer->Height < rect.y ? framebuffer->Height : rect.y;
	size_t ex = framebuffer->Width < rect.x + rect.w ? framebuffer->Width : rect.x + rect.w;
	size_t ey = framebuffer->Height < rect.y + rect.h ? framebuffer->Height : rect.y + rect.h;

	struct LinearColor fillColorspaceAdjusted = GraphicsLinearToColorspace(fillColor, framebuffer->Colorspace);
	for (size_t y = sy; y < ey; ++y)
	{
		for (size_t x = sx; x < ex; ++x)
			GraphicsSetPixel(framebuffer, x, y, fillColorspaceAdjusted);
	}
}

void GraphicsDrawLine(struct Framebuffer* framebuffer, struct GraphicsPoint a, struct GraphicsPoint b, struct LinearColor color)
{
	if (!framebuffer)
		return;

	if (a.x >= framebuffer->Width && b.x >= framebuffer->Width ||
		a.y >= framebuffer->Height && b.y >= framebuffer->Height)
		return;

	size_t sx = a.x < b.x ? a.x : b.x;
	size_t sy = a.y < b.y ? a.y : b.y;
	size_t ex = a.x < b.x ? b.x : a.x;
	size_t ey = a.y < b.y ? b.y : a.y;
	size_t dx = ex - sx;
	size_t dy = ey - sy;
	if (dx >= dy) // We want to set pixels following the line in the axis with greatest effect
	{
		ssize_t D = 2 * dy - dx;
		size_t  y = sy;
		ex        = framebuffer->Width < ex ? framebuffer->Width : ex;
		for (size_t x = sx; x <= ex; ++x)
		{
			GraphicsSetPixel(framebuffer, x, y, color);
			if (D > 0)
			{
				++y;
				D += (2 * (dy - dx));
			}
			D += 2 * dy;
		}
	}
	else
	{
		size_t D = 2 * dx - dy;
		size_t x = sx;
		ey       = framebuffer->Height < ey ? framebuffer->Height : ey;
		for (size_t y = sy; y <= ey; ++y)
		{
			GraphicsSetPixel(framebuffer, x, y, color);
			if (D > 0)
			{
				++x;
				D += (2 * (dx - dy));
			}
			D += 2 * dx;
		}
	}
}

void GraphicsDrawText(struct Framebuffer* framebuffer, struct GraphicsPoint pos, const char* text, size_t count, struct LinearColor color)
{
	if (!framebuffer)
		return;

	void* kernelPageTable = GetKernelPageTable();

	size_t currentX = pos.x;
	size_t currentY = pos.y;
	for (size_t i = 0; i < count; ++i)
	{
		char c = text[i]; // Decode UTF-8
		switch (c)
		{
		case '\r': currentX = pos.x; continue;
		case '\n':
			currentX  = pos.x + 2;
			currentY += g_FontHeight + 2;
			continue;
		case '\t': currentX += 4 * g_FontWidth + 2; continue;
		case ' ': currentX += g_FontWidth + 2; continue;
		}

		struct FontCharacter* character = nullptr;
		if (!VMMTranslate(kernelPageTable, &g_FontCharacters[(uint32_t) c]))
		{
			if (!VMMTranslate(kernelPageTable, &g_FontCharacters[0xFFFD]))
			{
				currentX += g_FontWidth + 2;
				continue;
			}
			character = &g_FontCharacters[0xFFFD];
		}
		else
		{
			character = &g_FontCharacters[(uint32_t) c];
		}
		if (character->Width == 0 && character->BitmapAddress == 0)
		{
			if (!VMMTranslate(kernelPageTable, &g_FontCharacters[0xFFFD]))
			{
				currentX += g_FontWidth + 2;
				continue;
			}
			character = &g_FontCharacters[0xFFFD];
		}

		size_t paddedWidth = ((size_t) character->Width * g_FontWidth + 7) & ~7;
		for (size_t y = 0; y < g_FontHeight; ++y)
		{
			size_t bitOffset = y * paddedWidth;
			for (size_t x = 0; x < character->Width * g_FontWidth; ++x)
			{
				if ((((uint8_t*) character->BitmapAddress)[bitOffset >> 3] >> (bitOffset & 7)) & 1)
					GraphicsSetPixel(framebuffer, currentX + x, currentY + y, color);
				++bitOffset;
			}
		}
		currentX += character->Width * g_FontWidth + 2;
	}
}