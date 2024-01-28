#include "Args/Args.hpp"
#include "BMP.hpp"
#include "FontFormat.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string_view>

struct FontGenOptions
{
	bool Verbose = false;

	std::filesystem::path InputFilepath;
	std::filesystem::path OutputFilepath;
};

struct Bitmap
{
	bool                 Loaded      = false;
	uint32_t             Width       = 0;
	uint32_t             Height      = 0;
	uint32_t             BitDepth    = 0;
	size_t               PaddedWidth = 0;
	std::vector<uint8_t> Bits;
};

struct Character
{
	uint8_t              Width;
	std::vector<uint8_t> Bits;

	size_t PaddedWidth;
	size_t OutputOffset;
};

struct FontGenState
{
	FontGenOptions Options;

	uint8_t CharWidth  = 0;
	uint8_t CharHeight = 0;
	uint8_t BitDepth   = 0;

	Bitmap                        SourceBitmap;
	std::map<uint32_t, Character> CharMap;
};

struct CommandSpec
{
	std::string Name;
	std::size_t MinArgs;
	std::size_t MaxArgs;
	void (*InvokeFn)(FontGenState& state, const std::vector<std::string_view>& args);
};

std::vector<CommandSpec> g_Commands;

void RegisterCommand(CommandSpec command)
{
	auto itr = std::find_if(g_Commands.begin(), g_Commands.end(), [&command](const CommandSpec& com) -> bool { return com.Name == command.Name; });
	if (itr != g_Commands.end())
	{
		std::cerr << "FontGen ERROR: command '" << command.Name << "' already registered\n";
		exit(1);
	}
	g_Commands.emplace_back(std::move(command));
}

static void OnFontDef(FontGenState& state, const std::vector<std::string_view>& args);
static void OnCharDef(FontGenState& state, const std::vector<std::string_view>& args);
static void OnLoad(FontGenState& state, const std::vector<std::string_view>& args);

static void BitCopy(void* output, size_t outputBit, const void* input, size_t inputBit, size_t bitCount);

int main(int argc, char** argv)
{
	FontGenState state {};

	Args::RegisterOption({ .Name = "", .MinArgs = 1, .MaxArgs = 1 }, { .Syntax = "<input filepath>", .Description = "Specify input filepath" });
	Args::RegisterOption({ .Name = "verbose" }, { .Syntax = "", .Description = "Enable verbose output" });
	Args::RegisterOption({ .Name = "output", .MinArgs = 1, .MaxArgs = 1, .MinCount = 1 }, { .Syntax = "<filepath>", .Description = "Specify output filepath" });
	{
		auto argOptions = Args::Handle(argc, argv);
		if (argOptions.ShouldExit)
			return argOptions.ExitCode;
		if (!argOptions["output"])
		{
			std::cerr << "FontGen ERROR: output filepath not specified\n";
			return 1;
		}
		state.Options.Verbose        = argOptions["verbose"];
		state.Options.InputFilepath  = std::filesystem::absolute(argOptions[""][0][0]);
		state.Options.OutputFilepath = std::filesystem::absolute(argOptions["output"][0][0]);
	}

	if (state.Options.Verbose)
	{
		std::cout << "Reading from input file " << state.Options.InputFilepath << '\n'
				  << "Writing to output file " << state.Options.OutputFilepath << '\n';
	}

	RegisterCommand({ .Name = "fontdef", .MinArgs = 1, .MaxArgs = ~0ULL, .InvokeFn = &OnFontDef });
	RegisterCommand({ .Name = "chardef", .MinArgs = 1, .MaxArgs = ~0ULL, .InvokeFn = &OnCharDef });
	RegisterCommand({ .Name = "load", .MinArgs = 1, .MaxArgs = 1, .InvokeFn = &OnLoad });

	std::string fontFileContent;
	{
		std::ifstream fontFile(state.Options.InputFilepath, std::ios::ate);
		if (!fontFile)
		{
			std::cerr << "FontGen ERROR: failed to open font file " << state.Options.InputFilepath << '\n';
			return 1;
		}

		fontFileContent.resize(fontFile.tellg());
		fontFile.seekg(0);
		fontFile.read(fontFileContent.data(), fontFileContent.size());
		fontFileContent.resize(fontFile.tellg());
		fontFile.close();
	}

	auto origPath = std::filesystem::current_path();
	std::filesystem::current_path(state.Options.InputFilepath.parent_path());

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

		auto itr = std::find_if(g_Commands.begin(), g_Commands.end(), [command](const CommandSpec& com) -> bool { return com.Name == command; });
		if (itr == g_Commands.end())
		{
			std::cerr << "FontGen ERROR: command '" << command << "' unknown\n";
			continue;
		}

		if (args.size() < itr->MinArgs)
		{
			std::cerr << "FontGen ERROR: missing '" << (itr->MinArgs - args.size()) << "' arguments for command '" << command << "'\n";
			continue;
		}
		if (args.size() > itr->MaxArgs)
		{
			std::cerr << "FontGen ERROR: '" << (args.size() - itr->MaxArgs) << "' too many arguments for command '" << command << "'\n";
			continue;
		}

		itr->InvokeFn(state, args);
	}

	std::filesystem::current_path(origPath);

	{
		std::ofstream outputFile(state.Options.OutputFilepath, std::ios::binary);
		if (!outputFile)
		{
			std::cerr << "FontGen ERROR: failed to open output file " << state.Options.OutputFilepath << '\n';
			return 1;
		}

		size_t          bitmapStart    = sizeof(FontFileHeader) - sizeof(FontFileCharacterLUT) + state.CharMap.size() * sizeof(FontFileCharacterLUT);
		size_t          currentBitmap  = bitmapStart;
		FontFileHeader* fontFileHeader = (FontFileHeader*) new uint8_t[bitmapStart];
		fontFileHeader->CharWidth      = state.CharWidth;
		fontFileHeader->CharHeight     = state.CharHeight;
		fontFileHeader->BitDepth       = state.BitDepth;
		fontFileHeader->Pad0           = 0;
		fontFileHeader->Pad1           = 0;
		fontFileHeader->CharacterCount = state.CharMap.size();
		size_t charIndex               = 0;
		for (auto& [c, character] : state.CharMap)
		{
			auto& lut       = fontFileHeader->LUT[charIndex++];
			lut.Character   = (uint32_t) c;
			lut.Width       = character.Width;
			lut.Pad0        = 0;
			lut.PaddedWidth = (uint16_t) character.PaddedWidth;
			lut.Offset      = currentBitmap;
			currentBitmap  += character.PaddedWidth * state.CharHeight;

			character.OutputOffset = lut.Offset;
		}

		outputFile.write((const char*) fontFileHeader, bitmapStart);
		for (auto& [c, character] : state.CharMap)
		{
			outputFile.write((const char*) character.Bits.data(), character.PaddedWidth * state.CharHeight);

			if (state.Options.Verbose)
			{
				std::cout << "FontGen INFO: Character " << c << " (" << (uint16_t) character.Width << ")\n";
				uint8_t mask = (uint8_t) ~0U >> (8 - state.BitDepth);
				for (size_t y = 0; y < state.CharHeight; ++y)
				{
					uint8_t* line = character.Bits.data() + character.PaddedWidth * y;
					for (size_t x = 0; x < state.BitDepth * state.CharWidth * character.Width; x += state.BitDepth)
					{
						uint8_t temp = (line[x >> 3] >> (x & 7)) & mask;
						temp       <<= 8 - state.BitDepth;
						std::cout << "\x1B[38;2;" << (uint16_t) temp << ";" << (uint16_t) temp << ";" << (uint16_t) temp << "m\xE2\x96\x88\xE2\x96\x88";
					}
					std::cout << "\x1B[39m\n";
				}
			}
		}

		outputFile.close();
		delete[] (uint8_t*) fontFileHeader;
	}

	return 0;
}

void OnFontDef(FontGenState& state, const std::vector<std::string_view>& args)
{
	if (state.CharWidth != 0 ||
		state.CharHeight != 0 ||
		state.BitDepth != 0)
	{
		std::cerr << "FontGen ERROR: fontdef already executed skipping\n";
		return;
	}

	int32_t width    = 0;
	int32_t height   = 0;
	int32_t bitdepth = 0;

	for (auto arg : args)
	{
		if (arg.starts_with("size:"))
		{
			arg = arg.substr(5);
			if (std::sscanf(arg.data(), "%dx%d", &width, &height) < 0)
				std::cout << "FontGen WARN: fontdef failed to parse 'size:' argument '" << arg << "' skipping\n";
		}
		else if (arg.starts_with("bpp:"))
		{
			arg = arg.substr(4);
			if (std::sscanf(arg.data(), "%d", &bitdepth) < 0)
				std::cout << "FontGen WARN: fontdef failed to parse 'bpp:' argument '" << arg << "' skipping\n";
		}
		else
		{
			std::cout << "FontGen WARN: fontdef unknown argument '" << arg << "' skipping\n";
		}
	}
	if (bitdepth != 1 && bitdepth != 2 && bitdepth != 4 && bitdepth != 8)
	{
		std::cerr << "FontGen ERROR: fontdef 'bpp:' not specified or not 1, 2, 4 or 8\n";
		return;
	}
	if (width < 1 || width > 128 || height < 1 || height > 128)
	{
		std::cerr << "FontGen ERROR: fontdef 'size:' not specified or outside valid range of 1 <= w <= 128 and 1 <= h <= 128\n";
		return;
	}

	if (bitdepth != 1)
	{
		std::cerr << "FontGen ERROR: fontdef 'bpp:' must be 1 at the moment :P\n";
		return;
	}
	if (width != 8 || height != 16)
	{
		std::cerr << "FontGen ERROR: fontdef 'size:' must be 8x16 at the moment :P\n";
		return;
	}

	state.CharWidth  = (uint8_t) width;
	state.CharHeight = (uint8_t) height;
	state.BitDepth   = (uint8_t) bitdepth;
}

void OnCharDef(FontGenState& state, const std::vector<std::string_view>& args)
{
	if (!state.SourceBitmap.Loaded)
	{
		std::cerr << "FontGen ERROR: chardef requires a previous load command\n";
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
				std::cout << "FontGen WARN: chardef failed to parse 'chars:' argument '" << arg << "' skipping\n";
		}
		else if (arg.starts_with("width:"))
		{
			arg = arg.substr(6);
			if (std::sscanf(arg.data(), "%d", &width) < 0)
				std::cout << "FontGen WARN: chardef failed to parse 'width:' argument '" << arg << "' skipping\n";
		}
		else if (arg.starts_with("dx:"))
		{
			arg = arg.substr(3);
			if (std::sscanf(arg.data(), "%d", &dx) < 0)
				std::cout << "FontGen WARN: chardef failed to parse 'dx:' argument '" << arg << "' skipping\n";
		}
		else if (arg.starts_with("img:"))
		{
			arg = arg.substr(4);
			if (std::sscanf(arg.data(), "%d,%d->%d,%d", &imgX, &imgY, &imgXE, &imgYE) < 0)
				std::cout << "FontGen WARN: chardef failed to parse 'img:' argument '" << arg << "' skipping\n";
		}
		else
		{
			std::cout << "FontGen WARN: chardef unknown argument '" << arg << "' skipping\n";
		}
	}
	if (firstChar < 0 || firstChar > 0x11'000 ||
		lastChar < 0 || lastChar > 0x11'000)
	{
		std::cerr << "FontGen ERROR: chardef 'chars:' not specified or outside valid range 0 <= c <= 1114112\n";
		return;
	}
	if (width < 1 || width > 8)
	{
		std::cerr << "FontGen ERROR: chardef 'width:' not specified or outside valid range 1 <= w <= 8\n";
		return;
	}
	if (imgX < 0 || imgX > state.SourceBitmap.Width ||
		imgY < 0 || imgY > state.SourceBitmap.Height ||
		imgXE < 0 || imgXE > state.SourceBitmap.Width ||
		imgYE < 0 || imgYE > state.SourceBitmap.Height)
	{
		std::cerr << "FontGen ERROR: chardef 'img:' not specified or outside valid range 0 <= x <= " << state.SourceBitmap.Width << ", 0 <= y <= " << state.SourceBitmap.Height << '\n';
		return;
	}
	if (dx != 0 && dx < (uint32_t) width * state.CharWidth)
	{
		std::cerr << "FontGen ERROR: chardef 'dx:' outside valid range " << ((uint32_t) width * state.CharWidth) << " <= x || x == 0\n";
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

	size_t paddedWidth = ((size_t) state.BitDepth * state.CharWidth * width + 7) / 8;

	size_t charX = imgX;
	size_t charY = imgY;
	for (size_t i = (size_t) firstChar; i <= lastChar; ++i)
	{
		if (charY + state.CharHeight > imgYE)
		{
			std::cerr << "FontGen ERROR: chardef attempting to add characters without bitmaps from " << i << " to " << lastChar << " will be discarded\n";
			break;
		}
		char32_t c = (char32_t) i;
		if (state.CharMap.contains(c))
			std::cout << "FontGen WARN: chardef character '" << i << "' already has been created overriding\n";
		auto& character       = state.CharMap[c];
		character.Width       = (uint8_t) width;
		character.PaddedWidth = paddedWidth;
		character.Bits.clear();
		character.Bits.resize(paddedWidth * state.CharHeight, 0);
		for (size_t y = 0; y < state.CharHeight; ++y)
			BitCopy(character.Bits.data(), y * paddedWidth * 8, state.SourceBitmap.Bits.data(), state.SourceBitmap.BitDepth * charX + (charY + y) * state.SourceBitmap.PaddedWidth * 8, state.BitDepth * state.CharWidth * width);
		charX += dx;
		if (charX + state.CharWidth * width > imgXE)
		{
			charX  = imgX;
			charY += state.CharHeight;
		}
	}
}

void OnLoad(FontGenState& state, const std::vector<std::string_view>& args)
{
	if (state.CharWidth == 0 || state.CharHeight == 0 || state.BitDepth == 0)
	{
		std::cerr << "FontGen ERROR: load requires a previous fontdef command\n";
		return;
	}

	std::filesystem::path bmpFilePath = std::filesystem::absolute(args[0]);
	if (!std::filesystem::exists(bmpFilePath))
	{
		std::cerr << "FontGen ERROR: load couldn't find file specified" << bmpFilePath << '\n';
		return;
	}

	std::vector<uint8_t> fileData;
	{
		std::ifstream file(bmpFilePath, std::ios::binary | std::ios::ate);
		if (!file)
		{
			std::cerr << "FontGen ERROR: load couuldn't open file specified " << bmpFilePath << '\n';
			return;
		}

		fileData.resize(file.tellg());
		file.seekg(0);
		file.read((char*) fileData.data(), fileData.size());
		file.close();
	}

	if (fileData.size() < 2 + sizeof(BMPHeader))
	{
		std::cerr << "FontGen ERROR: load bmp file " << bmpFilePath << " too small\n";
		return;
	}

	if (fileData[0] != 0x42 || fileData[1] != 0x4D)
	{
		std::cerr << "FontGen ERROR: load bmp file " << bmpFilePath << " incorrect identifier " << (char) fileData[0] << (char) fileData[1] << ", expected BM\n";
		return;
	}

	BMPHeader* header = (BMPHeader*) (fileData.data() + 2);
	if (header->Compression != 0)
	{
		std::cerr << "FontGen ERROR: load bmp file " << bmpFilePath << " is compressed, FontGen does not support that\n";
		return;
	}
	if (header->Bpp != state.BitDepth)
	{
		std::cerr << "FontGen ERROR: load bmp file " << bmpFilePath << " contains a different bitdepth than that specified in fontdef '" << header->Bpp << "' vs '" << state.BitDepth << "'\n";
		return;
	}

	auto& bitmap       = state.SourceBitmap;
	bitmap.Loaded      = true;
	bitmap.Width       = header->Width;
	bitmap.Height      = header->Height;
	bitmap.BitDepth    = header->Bpp;
	bitmap.PaddedWidth = (bitmap.BitDepth * bitmap.Width + 7) / 8;
	bitmap.Bits.clear();
	bitmap.Bits.resize(bitmap.PaddedWidth * bitmap.Height, 0);

	size_t    bmpPaddedWidth = ((size_t) bitmap.BitDepth * bitmap.Width + 31) / 32 * 4;
	uint32_t* bmpColorTable  = (uint32_t*) (fileData.data() + 2 + sizeof(BMPHeader));
	uint8_t*  bmpPixelArray  = fileData.data() + header->Offset;
	for (size_t y = 0; y < bitmap.Height; ++y)
		BitCopy(bitmap.Bits.data(), y * bitmap.PaddedWidth * 8, bmpPixelArray, (bitmap.Height - y - 1) * bmpPaddedWidth * 8, bitmap.PaddedWidth * 8);

	if (state.BitDepth == 1 || state.BitDepth == 2 || state.BitDepth == 4 || state.BitDepth == 8)
	{
		uint8_t transform[256];
		memset(transform, 0, 256);
		uint8_t mask  = (uint8_t) ~0U >> (8 - state.BitDepth);
		size_t  count = 8 / state.BitDepth;
		for (size_t i = 0; i < 256; ++i)
		{
			uint8_t value = 0;
			for (size_t j = 0; j < count; ++j)
			{
				uint32_t col       = bmpColorTable[(i >> (j * state.BitDepth)) & mask];
				double   luminance = 0.2126 * ((col >> 16) & 0xFF) / 255.0 +
								   0.7152 * ((col >> 8) & 0xFF) / 255.0 +
								   0.0722 * (col & 0xFF) / 255.0;
				uint8_t curValue = (uint8_t) (luminance * state.BitDepth + 0.5);
				value           |= curValue << ((count - j - 1) * state.BitDepth);
			}
			transform[i] = value;
		}

		for (size_t i = 0; i < bitmap.Bits.size(); ++i)
			bitmap.Bits[i] = transform[bitmap.Bits[i]];
	}

	if (state.Options.Verbose)
	{
		std::cout << "FontGen INFO: Loaded BMP file " << bmpFilePath << " (" << bitmap.Width << 'x' << bitmap.Height << " at " << bitmap.BitDepth << " bpp)\n";
		uint8_t mask = (uint8_t) ~0U >> (8 - state.BitDepth);
		for (size_t y = 0; y < bitmap.Height; ++y)
		{
			uint8_t* line = bitmap.Bits.data() + bitmap.PaddedWidth * y;
			for (size_t x = 0; x < bitmap.BitDepth * bitmap.Width; x += bitmap.BitDepth)
			{
				uint8_t temp = (line[x >> 3] >> (x & 7)) & mask;
				temp       <<= 8 - state.BitDepth;
				std::cout << "\x1B[38;2;" << (uint16_t) temp << ";" << (uint16_t) temp << ";" << (uint16_t) temp << "m\xE2\x96\x88\xE2\x96\x88";
			}
			std::cout << '\n';
		}
	}
}

void BitCopy(void* output, size_t outputBit, const void* input, size_t inputBit, size_t bitCount)
{
	uint8_t*       pO = (uint8_t*) output;
	const uint8_t* pI = (const uint8_t*) input;

	pO             += outputBit >> 3;
	pI             += inputBit >> 3;
	size_t writeBit = outputBit & 7;
	size_t readBit  = inputBit & 7;

	if (bitCount <= 64)
	{
		for (size_t i = 0; i < bitCount; ++i)
		{
			uint8_t temp       = (pI[readBit >> 3] >> (readBit & 7)) & 1;
			pO[writeBit >> 3] |= temp << (writeBit & 7);
			++readBit;
			++writeBit;
		}
		return;
	}

	uint8_t*       pOEnd = pO + ((bitCount + 7) >> 3);
	const uint8_t* pIEnd = pI + ((bitCount + 7) >> 3);

	uint16_t temp = *pI++;
	temp         |= (*pI++) << 8;
	temp        >>= readBit;
	*pO          &= ~(uint8_t) (~0U << writeBit);
	*pO++        |= (uint8_t) (temp << writeBit);
	temp        >>= 8 - writeBit;
	readBit       = 8 - ((readBit + writeBit) & 7);
	while (pO != pOEnd && pI != pIEnd)
	{
		temp  |= (uint16_t) (*pI++) << readBit;
		*pO++  = (uint8_t) temp;
		temp >>= 8;
	}
	uint8_t readMask  = (uint8_t) ~0U >> (7 - ((readBit + bitCount - 1) & 7));
	uint8_t writeMask = (uint8_t) ~0U >> (7 - ((writeBit + bitCount - 1) & 7));
	temp             |= (uint16_t) (*pI & readMask) << readBit;
	*pO              &= ~writeMask;
	*pO              |= (uint8_t) temp & writeMask;
}