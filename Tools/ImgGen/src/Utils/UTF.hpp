#pragma once

#include <string>

namespace UTF
{
	int8_t UTF8DecodeCodepoint(std::string_view utf8, char32_t& codepoint);
	int8_t UTF16DecodeCodepoint(std::u16string_view utf16, char32_t& codepoint);
	bool   UTF8EncodeCodepoint(std::string& utf8, char32_t codepoint);
	bool   UTF16EncodeCodepoint(std::u16string& utf16, char32_t codepoint);

	std::u16string UTF8ToUTF16(std::string_view utf8);
	std::u32string UTF8ToUTF32(std::string_view utf8);
	std::string    UTF16ToUTF8(std::u16string_view utf16);
	std::u32string UTF16ToUTF32(std::u16string_view utf16);
	std::string    UTF32ToUTF8(std::u32string_view utf32);
	std::u16string UTF32ToUTF16(std::u32string_view utf32);

	char32_t UTF32ToLower(char32_t codepoint);
} // namespace UTF