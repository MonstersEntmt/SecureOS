#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct OutputFileCharLUT
{
	char32_t Character;
	uint8_t  Width;
	uint8_t  Pad[3];
	size_t   Offset;
};

struct OutputFileHeader
{
	uint8_t  CharWidth;
	uint8_t  CharHeight;
	uint8_t  Bitdepth;
	uint8_t  Pad0;
	uint32_t Pad1;
	uint64_t CharacterCount;
};

struct Bitmap
{
	bool                 Loaded = false;
	uint32_t             Width = 0, Height = 0, Bitdepth = 0;
	size_t               PaddedWidth = 0;
	std::vector<uint8_t> Bits;
};

struct Character
{
	uint8_t              Width;
	std::vector<uint8_t> Bitmap;

	size_t OutputOffset;
};

struct ProgramState
{
	uint8_t CharWidth = 0, CharHeight = 0, Bitdepth = 0;
	Bitmap  SourceBitmap;

	std::map<char32_t, Character> CharMap;
};

struct CommandSpec
{
	std::string Name;
	std::size_t MinArgs;
	std::size_t MaxArgs;
	void (*InvokeFn)(ProgramState& state, const std::vector<std::string_view>& args);
};

std::vector<CommandSpec> g_Commands;

static void OnFontDef(ProgramState& state, const std::vector<std::string_view>& args);
static void OnLoad(ProgramState& state, const std::vector<std::string_view>& args);
static void OnDef(ProgramState& state, const std::vector<std::string_view>& args);

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::cerr << "Expected 2 arguments, <font file> <output file>\n";
		return 1;
	}

	std::string fontFileContent;
	{
		std::ifstream fontFile(argv[1], std::ios::ate);
		if (!fontFile)
		{
			std::cerr << "Failed to open font file '" << argv[1] << "'\n";
			return 1;
		}

		fontFileContent.resize(fontFile.tellg());
		fontFile.seekg(0);
		fontFile.read(fontFileContent.data(), fontFileContent.size());
		fontFileContent.resize(fontFile.tellg());
		fontFile.close();
	}

	auto origcwd = std::filesystem::current_path();
	std::filesystem::current_path(std::filesystem::canonical(std::filesystem::path { argv[1] }.parent_path()));

	g_Commands.emplace_back(CommandSpec { .Name = "fontdef", .MinArgs = 1, .MaxArgs = ~0ULL, .InvokeFn = &OnFontDef });
	g_Commands.emplace_back(CommandSpec { .Name = "load", .MinArgs = 1, .MaxArgs = 1, .InvokeFn = &OnLoad });
	g_Commands.emplace_back(CommandSpec { .Name = "def", .MinArgs = 1, .MaxArgs = ~0ULL, .InvokeFn = &OnDef });

	ProgramState state;

	size_t fontFileOffset = 0;
	while (fontFileOffset < fontFileContent.size())
	{
		size_t lineEnd = std::min<size_t>(fontFileContent.size(), fontFileContent.find_first_of('\n', fontFileOffset));
		size_t offset  = fontFileOffset;
		fontFileOffset = std::min<size_t>(fontFileContent.size(), fontFileContent.find_first_not_of('\n', lineEnd + 1));

		std::vector<std::string_view> args;
		while (offset < lineEnd)
		{
			size_t argEnd = std::min<size_t>(lineEnd, fontFileContent.find_first_of(' ', offset));
			args.emplace_back(std::string_view { fontFileContent }.substr(offset, argEnd - offset));
			if (argEnd >= lineEnd)
				break;
			offset = std::min<size_t>(lineEnd, fontFileContent.find_first_not_of(' ', argEnd + 1));
		}
		std::string_view command = args[0];
		args.erase(args.begin());

		auto itr = std::find_if(g_Commands.begin(), g_Commands.end(), [command](const CommandSpec& spec) -> bool { return spec.Name == command; });
		if (itr == g_Commands.end())
		{
			std::cerr << "Command '" << command << "' does not exist\n";
			continue;
		}

		if (args.size() < itr->MinArgs)
		{
			std::cerr << "Too few arguments given, need at least " << itr->MinArgs << '\n';
			continue;
		}
		if (args.size() > itr->MaxArgs)
		{
			std::cerr << "Too many arguments given, needs at most " << itr->MaxArgs << '\n';
			continue;
		}

		itr->InvokeFn(state, args);
	}

	std::filesystem::current_path(origcwd);

	{
		std::ofstream outputFile(argv[2], std::ios::binary);
		if (!outputFile)
		{
			std::cerr << "Failed to open output file '" << argv[2] << "'\n";
			return 1;
		}

		OutputFileHeader header {};
		header.CharWidth      = state.CharWidth;
		header.CharHeight     = state.CharHeight;
		header.Bitdepth       = state.Bitdepth;
		header.Pad0           = 0;
		header.Pad1           = 0;
		header.CharacterCount = state.CharMap.size();
		outputFile.write((const char*) &header, sizeof(header));
		size_t bitmapStart = sizeof(header) + state.CharMap.size() * sizeof(OutputFileCharLUT);
		for (auto& [c, character] : state.CharMap)
		{
			OutputFileCharLUT charLUT {};
			charLUT.Character      = c;
			charLUT.Width          = character.Width;
			charLUT.Pad[0]         = 0;
			charLUT.Pad[1]         = 0;
			charLUT.Pad[2]         = 0;
			charLUT.Offset         = bitmapStart;
			character.OutputOffset = bitmapStart;
			size_t paddedWidth     = ((size_t) state.Bitdepth * character.Width * state.CharWidth + 7) / 8;
			bitmapStart           += paddedWidth * state.CharHeight;
			outputFile.write((const char*) &charLUT, sizeof(charLUT));
		}
		// std::cout << "First bitmap starts at: " << outputFile.tellp() << '\n';
		for (auto& [c, character] : state.CharMap)
		{
			// std::cout << (uint32_t) c << " at " << outputFile.tellp() << " with expected " << character.OutputOffset << ":\n";
			size_t paddedWidth = ((size_t) state.Bitdepth * character.Width * state.CharWidth + 7) / 8;
			outputFile.write((const char*) character.Bitmap.data(), paddedWidth * state.CharHeight);
			// for (size_t y = 0; y < state.CharHeight; ++y)
			// {
			// 	for (size_t x = 0; x < state.Bitdepth * character.Width * state.CharWidth; x += state.Bitdepth)
			// 	{
			// 		size_t  bitOffset = x + y * paddedWidth * 8;
			// 		uint8_t bit       = (character.Bitmap[bitOffset / 8] >> (bitOffset % 8)) & 1;
			// 		if (bit)
			// 			std::cout << "██";
			// 		else
			// 			std::cout << "  ";
			// 	}
			// 	std::cout << '\n';
			// }
		}

		outputFile.close();
	}

	return 0;
}

void OnFontDef(ProgramState& state, const std::vector<std::string_view>& args)
{
	int32_t width = 0, height = 0;
	int32_t bpp = 0;

	for (auto arg : args)
	{
		if (arg.starts_with("size:"))
		{
			arg = arg.substr(5);
			if (std::sscanf(arg.data(), "%dx%d", &width, &height) < 0)
				std::cerr << "fontdef -> Failed to parse 'size:' argument '" << arg << "' skipping\n";
		}
		else if (arg.starts_with("bpp:"))
		{
			arg = arg.substr(4);
			if (std::sscanf(arg.data(), "%d", &bpp) < 0)
				std::cerr << "fontdef -> Failed to parse 'bpp:' argument '" << arg << "' skipping\n";
		}
		else
		{
			std::cerr << "fontdef -> Unknown arg '" << arg << "' skipping\n";
		}
	}
	if (bpp < 1 || bpp > 32)
	{
		std::cerr << "fontdef -> 'bpp:' not specified or outside valid range of 1 <= b <= 32\n";
		return;
	}
	if (bpp != 1)
	{
		std::cerr << "fontdef -> 'bpp:' must be 1 at the moment\n";
		return;
	}
	if (width < 1 || width > 128 || height < 1 || height > 128)
	{
		std::cerr << "fontdef -> 'size:' not specified or outside valid range of 1 <= w <= 128 and 1 <= h <= 128\n";
		return;
	}

	state.CharWidth  = (uint8_t) width;
	state.CharHeight = (uint8_t) height;
	state.Bitdepth   = (uint8_t) bpp;
}

struct BMPHeader
{
	uint16_t iden;
	uint16_t sizeLow;
	uint16_t sizeHigh;
	uint16_t reserved1;
	uint16_t reserved2;
	uint16_t offsetLow;
	uint16_t offsetHigh;

	uint16_t headerSizeLow;
	uint16_t headerSizeHigh;
	uint16_t widthLow;
	uint16_t widthHigh;
	uint16_t heightLow;
	uint16_t heightHigh;
	uint16_t planes;
	uint16_t bpp;
	uint16_t compressionLow;
	uint16_t compressionHigh;
	uint16_t imageSizeLow;
	uint16_t imageSizeHigh;
	uint16_t resXLow;
	uint16_t resXHigh;
	uint16_t resYLow;
	uint16_t resYHigh;
	uint16_t colorPaletteLow;
	uint16_t colorPaletteHigh;
	uint16_t importantColorsLow;
	uint16_t importantColorsHigh;
};

void OnLoad(ProgramState& state, const std::vector<std::string_view>& args)
{
	if (state.CharWidth == 0 || state.CharHeight == 0 || state.Bitdepth == 0)
	{
		std::cerr << "load -> Requires fontdef command prior\n";
		return;
	}

	std::filesystem::path bmpFilePath = std::filesystem::absolute(args[0]);

	if (!std::filesystem::exists(bmpFilePath))
	{
		std::cerr << "load -> Couldn't find file specified " << bmpFilePath << '\n';
		return;
	}

	std::ifstream file(bmpFilePath, std::ios::binary | std::ios::ate);
	if (!file)
	{
		std::cerr << "load -> Couldn't open file specified " << bmpFilePath << '\n';
		return;
	}

	std::vector<uint8_t> fileData(file.tellg());
	file.seekg(0);
	file.read((char*) fileData.data(), fileData.size());
	file.close();

	BMPHeader* header = (BMPHeader*) fileData.data();
	if (header->iden != 0x4D42)
	{
		std::cerr << "load -> File is not a valid bitmap file " << bmpFilePath << '\n';
		return;
	}
	if (header->compressionHigh != 0 || header->compressionLow != 0)
	{
		std::cerr << "load -> Bitmap file is compressed, tool doesn't support that " << bmpFilePath << '\n';
		return;
	}
	if (header->bpp != state.Bitdepth)
	{
		std::cerr << "load -> Bitmap file contains a different bpp than that specified in fontdef '" << header->bpp << "' vs '" << state.Bitdepth << "'\n";
		return;
	}

	state.SourceBitmap.Loaded      = true;
	state.SourceBitmap.Width       = header->widthHigh << 16 | header->widthLow;
	state.SourceBitmap.Height      = header->heightHigh << 16 | header->heightLow;
	state.SourceBitmap.Bitdepth    = header->bpp;
	state.SourceBitmap.PaddedWidth = (state.SourceBitmap.Bitdepth * state.SourceBitmap.Width + 7) / 8;
	size_t bmpPaddedWidth          = ((size_t) state.SourceBitmap.Bitdepth * state.SourceBitmap.Width + 31) / 32 * 4;
	state.SourceBitmap.Bits.clear();
	state.SourceBitmap.Bits.resize(state.SourceBitmap.PaddedWidth * state.SourceBitmap.Height);
	size_t    pixelArrayOffset = (header->offsetHigh << 16) | header->offsetLow;
	uint32_t* bmpColorTable    = (uint32_t*) (fileData.data() + sizeof(BMPHeader));
	uint8_t*  bmpPixelArray    = fileData.data() + pixelArrayOffset;
	for (size_t y = 0; y < state.SourceBitmap.Height; ++y)
	{
		size_t destOffset   = 8 * y * state.SourceBitmap.PaddedWidth;
		size_t sourceOffset = (state.SourceBitmap.Height - y - 1) * bmpPaddedWidth * 8;
		for (size_t x = 0; x < state.SourceBitmap.PaddedWidth * 8; ++x)
		{
			uint8_t  temp = (bmpPixelArray[sourceOffset >> 3] >> (sourceOffset & 7)) & 1;
			uint32_t col  = bmpColorTable[temp];
			if (((col >> 16) & 0xFF) > 0x7F ||
				((col >> 8) & 0xFF) > 0x7F ||
				(col & 0xFF) > 0x7F)
				temp = 1;
			else
				temp = 0;
			state.SourceBitmap.Bits[destOffset >> 3] |= temp << (destOffset & 7);
			++destOffset;
			++sourceOffset;
		}
	}
}

void OnDef(ProgramState& state, const std::vector<std::string_view>& args)
{
	if (!state.SourceBitmap.Loaded)
	{
		std::cerr << "def -> Requires load command prior\n";
		return;
	}

	int32_t firstChar = 0;
	int32_t lastChar  = 0;
	int32_t width     = 0;
	int32_t dx        = 0;
	int32_t imgX      = 0;
	int32_t imgY      = 0;
	int32_t imgXE     = 0;
	int32_t imgYE     = 0;

	for (auto arg : args)
	{
		if (arg.starts_with("chars:"))
		{
			arg = arg.substr(6);
			if (std::sscanf(arg.data(), "%d->%d", &firstChar, &lastChar) < 0)
				std::cerr << "def -> Failed to parse 'chars:' argument '" << arg << "' skipping\n";
		}
		else if (arg.starts_with("width:"))
		{
			arg = arg.substr(6);
			if (std::sscanf(arg.data(), "%d", &width) < 0)
				std::cerr << "def -> Failed to parse 'width:' argument '" << arg << "' skipping\n";
		}
		else if (arg.starts_with("dx:"))
		{
			arg = arg.substr(3);
			if (std::sscanf(arg.data(), "%d", &dx) < 0)
				std::cerr << "def -> Failed to parse 'dx:' argument '" << arg << "' skipping\n";
		}
		else if (arg.starts_with("img:"))
		{
			arg = arg.substr(4);
			if (std::sscanf(arg.data(), "%d,%d->%d,%d", &imgX, &imgY, &imgXE, &imgYE) < 0)
				std::cerr << "def -> Failed to parse 'img:' argument '" << arg << "' skipping\n";
		}
		else
		{
			std::cerr << "def -> Unknown arg '" << arg << "' skipping\n";
		}
	}
	if (firstChar < 0 || firstChar > 0x11'0000 ||
		lastChar < 0 || lastChar > 0x11'0000)
	{
		std::cerr << "def -> 'chars:' not specified or outside valid range 0 <= c <= 1114112\n";
		return;
	}
	if (width < 1 || width > 8)
	{
		std::cerr << "def -> 'width:' not specified or outside valid range 1 <= w <= 8\n";
		return;
	}
	if (imgX < 0 || imgX > state.SourceBitmap.Width ||
		imgY < 0 || imgY > state.SourceBitmap.Height ||
		imgXE < 0 || imgXE > state.SourceBitmap.Width ||
		imgYE < 0 || imgYE > state.SourceBitmap.Height)
	{
		std::cerr << "def -> 'img:' not specified or outside valid range 0 <= x <= " << state.SourceBitmap.Width << ", 0 <= y <= " << state.SourceBitmap.Height << '\n';
		return;
	}
	if (dx == 0)
		dx = (uint32_t) width * state.CharWidth;

	if (lastChar < firstChar)
		std::swap(firstChar, lastChar);
	if (imgXE < imgX)
		std::swap(imgX, imgXE);
	if (imgYE < imgY)
		std::swap(imgY, imgYE);

	size_t paddedWidth = ((size_t) state.Bitdepth * state.CharWidth * width + 7) / 8;

	size_t charX = imgX;
	size_t charY = imgY;
	for (size_t i = (size_t) firstChar; i <= lastChar; ++i)
	{
		if (charY + state.CharHeight > imgYE)
		{
			std::cerr << "def -> Attempting to add characters without bitmaps from " << i << " to " << lastChar << " will be discarded\n";
			break;
		}
		char32_t c = (char32_t) i;
		if (state.CharMap.contains(c))
			std::cout << "def -> Character map already contains a character in this range " << (uint32_t) c << " in " << firstChar << "->" << lastChar << " overriding character\n";
		auto& character = state.CharMap[c];
		character.Width = (uint8_t) width;
		character.Bitmap.clear();
		character.Bitmap.resize(paddedWidth * state.CharHeight, 0);
		for (size_t y = 0; y < state.CharHeight; ++y)
		{
			size_t destBit   = y * paddedWidth * 8;
			size_t sourceBit = state.SourceBitmap.Bitdepth * charX + (charY + y) * state.SourceBitmap.PaddedWidth * 8;
			for (size_t x = 0; x < state.CharWidth * width; ++x)
			{
				uint8_t temp                   = (state.SourceBitmap.Bits[sourceBit / 8] >> (7 - (sourceBit % 8))) & 1;
				character.Bitmap[destBit / 8] |= temp << (destBit % 8);
				destBit                       += state.Bitdepth;
				sourceBit                     += state.SourceBitmap.Bitdepth;
			}
		}
		charX += dx;
		if (charX + state.CharWidth * width > imgXE)
		{
			charX  = imgX;
			charY += state.CharHeight;
		}
	}
}