#include "UTF.h"

// TODO(MarcasRealAccount): Implement actual Unicode conversions

std::u16string UTF8ToUTF16(std::string_view utf8)
{
	std::u16string utf16;
	utf16.resize(utf8.size());
	for (size_t i = 0; i < utf8.size(); ++i)
	{
		utf16[i] = utf8[i] & 0x7F;
	}
	return utf16;
}

std::string UTF16ToUTF8(std::u16string_view utf16)
{
	std::string utf8;
	utf8.resize(utf16.size());
	for (size_t i = 0; i < utf16.size(); ++i)
	{
		utf8[i] = utf16[i] & 0x7F;
	}
	return utf8;
}