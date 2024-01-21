#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct Bitmap
{
	uint32_t             width, height;
	std::vector<uint8_t> bits;
};

struct Character
{
	uint8_t              width, height;
	std::vector<uint8_t> bitmap;
};

struct ProgramState
{
	std::map<char32_t, Character> characterBitmaps;
	Bitmap                        sourceBitmap;
};

struct CommandSpec
{
	std::string Name;
	std::size_t MinArgs;
	std::size_t MaxArgs;
	void (*InvokeFn)(ProgramState& state, const std::vector<std::string_view>& args);
};

std::vector<CommandSpec> g_Commands;

static void OnLoad(ProgramState& state, const std::vector<std::string_view>& args);
static void OnDef(ProgramState& state, const std::vector<std::string_view>& args);

static bool ReadBitmap(const std::filesystem::path& bmpFile, Bitmap& bitmap);

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

		for (auto& [c, character] : state.characterBitmaps)
		{
			uint8_t bitdepth = 1;
			uint8_t zero     = 0;
			outputFile.write((const char*) &c, 4);
			outputFile.write((const char*) &character.width, 1);
			outputFile.write((const char*) &character.height, 1);
			outputFile.write((const char*) &bitdepth, 1);
			outputFile.write((const char*) &zero, 1);
			outputFile.write((const char*) character.bitmap.data(), (character.width * character.height * bitdepth + 7) / 8);
		}

		outputFile.close();
	}

	return 0;
}

void OnLoad(ProgramState& state, const std::vector<std::string_view>& args)
{
	std::filesystem::path bmpFilePath = std::filesystem::absolute(args[0]);
	if (!ReadBitmap(bmpFilePath, state.sourceBitmap))
		std::cerr << "Failed to read bitmap " << bmpFilePath << "\n";
}

void OnDef(ProgramState& state, const std::vector<std::string_view>& args)
{
	if (state.sourceBitmap.width == 0 || state.sourceBitmap.height == 0 || state.sourceBitmap.bits.empty())
	{
		std::cerr << "Can't define characters when no source bitmap is loaded\n";
		return;
	}

	uint32_t firstChar  = 0;
	uint32_t lastChar   = 0;
	uint32_t charWidth  = 0;
	uint32_t charHeight = 0;
	uint32_t imgX       = 0;
	uint32_t imgY       = 0;
	uint32_t imgXE      = 0;
	uint32_t imgYE      = 0;

	for (auto arg : args)
	{
		if (arg.starts_with("chars:"))
		{
			arg = arg.substr(6);
			if (std::sscanf(arg.data(), "%u->%u", &firstChar, &lastChar) < 0)
				std::cerr << "Failed to parse chars: argument '" << arg << "' skipping\n";
		}
		else if (arg.starts_with("size:"))
		{
			arg = arg.substr(5);
			if (std::sscanf(arg.data(), "%ux%u", &charWidth, &charHeight) < 0)
				std::cerr << "Failed to parse size: argument '" << arg << "' skipping\n";
		}
		else if (arg.starts_with("img:"))
		{
			arg = arg.substr(4);
			if (std::sscanf(arg.data(), "%u,%u->%u,%u", &imgX, &imgY, &imgXE, &imgYE) < 0)
				std::cerr << "Failed to parse img: argument '" << arg << "' skipping\n";
		}
		else
		{
			std::cerr << "Unknown arg '" << arg << "' skipping\n";
		}
	}
	charWidth  = std::min<uint32_t>(charWidth, 255);
	charHeight = std::min<uint32_t>(charHeight, 255);

	for (size_t i = firstChar; i <= lastChar; ++i)
	{
		char32_t c         = (char32_t) i;
		auto&    character = state.characterBitmaps[c];
		character.width    = (uint8_t) charWidth;
		character.height   = (uint8_t) charHeight;
		character.bitmap.resize((character.width + 7) / 8 * character.height);
		uint8_t lastMask = ((uint8_t) ~0U) >> charWidth % 8;
		for (size_t j = 0; j < charHeight; ++j)
		{
			size_t destOffset   = j * (charWidth + 7) / 8;
			size_t sourceOffset = imgX + (imgY + j) * (state.sourceBitmap.width + 7) / 8;
			memcpy(character.bitmap.data() + destOffset, state.sourceBitmap.bits.data() + sourceOffset, (charWidth + 7) / 8);
			character.bitmap[destOffset + (charWidth + 7) / 8 - 1] &= lastMask;
		}
	}
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

bool ReadBitmap(const std::filesystem::path& bmpFile, Bitmap& bitmap)
{
	std::ifstream file(bmpFile, std::ios::binary | std::ios::ate);
	if (!file)
		return false;

	std::vector<uint8_t> fileData;
	fileData.resize(file.tellg());
	file.seekg(0);
	file.read((char*) fileData.data(), fileData.size());
	file.close();

	struct BMPHeader* bmpHeader = (struct BMPHeader*) fileData.data();
	if (bmpHeader->iden != 0x4D42)
	{
		std::cerr << "BMP Header Iden is not BM\n";
		return false;
	}
	if (bmpHeader->compressionHigh != 0 || bmpHeader->compressionLow != 0)
	{
		std::cerr << "BMP is compressed\n";
		return false;
	}
	if (bmpHeader->bpp != 1)
	{
		std::cerr << "BMP is not 1 bpp\n";
		return false;
	}
	bitmap.width  = bmpHeader->widthHigh << 16 | bmpHeader->widthLow;
	bitmap.height = bmpHeader->heightHigh << 16 | bmpHeader->heightLow;
	bitmap.bits.resize((bitmap.width + 7) / 8 * 8 * bitmap.height);
	uint8_t* bmpPixelArray = fileData.data() + ((bmpHeader->offsetHigh << 16) | bmpHeader->offsetLow);
	for (size_t i = 0; i < bitmap.height; ++i)
	{
		size_t destOffset   = i * (bitmap.width + 7) / 8;
		size_t sourceOffset = i * (bitmap.width + 31) / 32 * 4;
		memcpy(bitmap.bits.data() + (i * bitmap.width + 7) / 8, bmpPixelArray + sourceOffset, (bitmap.width + 7) / 8);
	}
	return true;
}