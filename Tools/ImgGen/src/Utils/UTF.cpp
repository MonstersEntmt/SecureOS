#include "UTF.hpp"
#include "Utils/UTFCaseFold.hpp"
#include <algorithm>

namespace UTF
{
	enum class EUTF8CharType : uint8_t
	{
		Invalid = 0,
		Continuation,
		OneByte,
		TwoByte,
		ThreeByte,
		FourByte
	};

	enum class EUTF16CharType : uint8_t
	{
		OneChar = 0,
		LowSurrogate,
		HighSurrogate
	};

	static constexpr EUTF8CharType  s_UTF8LUT[64]  = { EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::OneByte, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::Continuation, EUTF8CharType::TwoByte, EUTF8CharType::TwoByte, EUTF8CharType::TwoByte, EUTF8CharType::TwoByte, EUTF8CharType::TwoByte, EUTF8CharType::TwoByte, EUTF8CharType::TwoByte, EUTF8CharType::TwoByte, EUTF8CharType::ThreeByte, EUTF8CharType::ThreeByte, EUTF8CharType::ThreeByte, EUTF8CharType::ThreeByte, EUTF8CharType::FourByte, EUTF8CharType::FourByte, EUTF8CharType::Invalid, EUTF8CharType::Invalid };
	static constexpr EUTF16CharType s_UTF16LUT[64] = { EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar, EUTF16CharType::OneChar };

	int8_t UTF8DecodeCodepoint(std::string_view utf8, char32_t& codepoint)
	{
		if (utf8.empty())
		{
			codepoint = 0xFFFD;
			return 0;
		}

		switch (s_UTF8LUT[utf8[0] >> 2])
		{
		case EUTF8CharType::OneByte:
			codepoint = utf8[0];
			return 1;
		case EUTF8CharType::TwoByte:
			if (utf8.size() < 2)
			{
				codepoint = 0xFFFD;
				return (int8_t) utf8.size();
			}
			if (s_UTF8LUT[utf8[1] >> 2] != EUTF8CharType::Continuation)
			{
				codepoint = 0xFFFD;
				return 1;
			}
			codepoint = ((utf8[0] & 0b0001'1111) << 6) | (utf8[1] & 0b0011'1111);
			return 2;
		case EUTF8CharType::ThreeByte:
			if (utf8.size() < 3)
			{
				codepoint = 0xFFFD;
				return (int8_t) utf8.size();
			}
			if (s_UTF8LUT[utf8[1] >> 2] != EUTF8CharType::Continuation)
			{
				codepoint = 0xFFFD;
				return 1;
			}
			if (s_UTF8LUT[utf8[2] >> 2] != EUTF8CharType::Continuation)
			{
				codepoint = 0xFFFD;
				return 2;
			}
			codepoint = ((utf8[0] & 0b0000'1111) << 12) | ((utf8[1] & 0b0011'1111) << 6) | (utf8[2] & 0b0011'1111);
			return 3;
		case EUTF8CharType::FourByte:
			if (utf8.size() < 4)
			{
				codepoint = 0xFFFD;
				return (int8_t) utf8.size();
			}
			if (s_UTF8LUT[utf8[1] >> 2] != EUTF8CharType::Continuation)
			{
				codepoint = 0xFFFD;
				return 1;
			}
			if (s_UTF8LUT[utf8[2] >> 2] != EUTF8CharType::Continuation)
			{
				codepoint = 0xFFFD;
				return 2;
			}
			if (s_UTF8LUT[utf8[3] >> 2] != EUTF8CharType::Continuation)
			{
				codepoint = 0xFFFD;
				return 3;
			}
			codepoint = ((utf8[0] & 0b0000'0111) << 18) | ((utf8[1] & 0b0011'1111) << 12) | ((utf8[2] & 0b0011'1111) << 6) | (utf8[3] & 0b0011'1111);
			return 4;
		case EUTF8CharType::Invalid:
		case EUTF8CharType::Continuation:
		default:
			codepoint = 0xFFFD;
			return -1;
		}
	}

	int8_t UTF16DecodeCodepoint(std::u16string_view utf16, char32_t& codepoint)
	{
		if (utf16.empty())
		{
			codepoint = 0xFFFD;
			return 0;
		}

		switch (s_UTF16LUT[utf16[0] >> 9])
		{
		case EUTF16CharType::OneChar:
			codepoint = (char32_t) utf16[0];
			return 1;
		case EUTF16CharType::HighSurrogate:
		{
			if (utf16.size() < 2)
			{
				codepoint = 0xFFFD;
				return utf16.size();
			}
			if (s_UTF16LUT[utf16[1] >> 9] != EUTF16CharType::LowSurrogate)
			{
				codepoint = 0xFFFD;
				return 1;
			}
			codepoint = ((uint32_t) (utf16[0] & 0b0011'1111'1111) << 10 | (utf16[1] & 0b0011'1111'1111)) + 0x1'0000;
			return 2;
		}
		case EUTF16CharType::LowSurrogate:
		default:
			codepoint = 0xFFFD;
			return -1;
		}
	}

	bool UTF8EncodeCodepoint(std::string& utf8, char32_t codepoint)
	{
		if (codepoint >= 0x11'0000)
		{
			// FFFD in utf8
			utf8.push_back(0b11101111);
			utf8.push_back(0b10111111);
			utf8.push_back(0b10111101);
			return false;
		}
		else if (codepoint >= 0x1'0000)
		{
			utf8.push_back((char) (0b1111'0000 | (codepoint >> 18)));
			utf8.push_back((char) (0b1000'0000 | ((codepoint >> 12) & 0b0011'1111)));
			utf8.push_back((char) (0b1000'0000 | ((codepoint >> 6) & 0b0011'1111)));
			utf8.push_back((char) (0b1000'0000 | (codepoint & 0b0011'1111)));
		}
		else if (codepoint >= 0x800)
		{
			utf8.push_back((char) (0b1110'0000 | (codepoint >> 12)));
			utf8.push_back((char) (0b1000'0000 | ((codepoint >> 6) & 0b0011'1111)));
			utf8.push_back((char) (0b1000'0000 | (codepoint & 0b0011'1111)));
		}
		else if (codepoint >= 0x80)
		{
			utf8.push_back((char) (0b1110'0000 | (codepoint >> 6)));
			utf8.push_back((char) (0b1000'0000 | (codepoint & 0b0011'1111)));
		}
		else
		{
			utf8.push_back((char) codepoint);
		}
		return true;
	}

	bool UTF16EncodeCodepoint(std::u16string& utf16, char32_t codepoint)
	{
		if ((codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint >= 0x11'0000)
		{
			utf16.push_back(0xFFFD);
			return false;
		}
		else if (codepoint >= 0x1'0000)
		{
			uint32_t codepointPrime = codepoint - 0x1'0000;
			utf16.push_back(0xD800 + (codepointPrime >> 10));
			utf16.push_back(0xDC00 + (codepointPrime & 0b0011'1111'1111));
		}
		else
		{
			utf16.push_back((char16_t) codepoint);
		}
		return true;
	}

	int8_t UTF8EncodeCodepointLength(char32_t codepoint)
	{
		if (codepoint >= 0x11'0000)
			return -3;
		else if (codepoint >= 0x1'0000)
			return 4;
		else if (codepoint >= 0x800)
			return 3;
		else if (codepoint >= 0x80)
			return 2;
		else
			return 1;
	}

	int8_t UTF16EncodeCodepointLength(char32_t codepoint)
	{
		if ((codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint >= 0x11'0000)
			return -1;
		else if (codepoint >= 0x1'0000)
			return 2;
		else
			return 1;
	}

	std::u16string UTF8ToUTF16(std::string_view utf8)
	{
		std::u16string utf16;
		utf16.reserve(utf8.size());
		bool isInvalid = false;
		while (!utf8.empty())
		{
			char32_t codepoint = 0xFFFD;
			int8_t   result    = UTF8DecodeCodepoint(utf8, codepoint);
			if (result < 0)
			{
				if (!isInvalid)
					utf16.push_back(0xFFFD);
				isInvalid = true;
				utf8      = utf8.substr(-result);
				continue;
			}

			isInvalid = codepoint == 0xFFFD;
			utf8      = utf8.substr(result);
			isInvalid = !UTF16EncodeCodepoint(utf16, codepoint);
		}
		return utf16;
	}

	std::u32string UTF8ToUTF32(std::string_view utf8)
	{
		std::u32string utf32;
		utf32.reserve(utf8.size());
		bool isInvalid = false;
		while (!utf8.empty())
		{
			char32_t codepoint = 0xFFFD;
			int8_t   result    = UTF8DecodeCodepoint(utf8, codepoint);
			if (result < 0)
			{
				if (!isInvalid)
					utf32.push_back(0xFFFD);
				isInvalid = true;
				utf8      = utf8.substr(-result);
				continue;
			}

			isInvalid = codepoint == 0xFFFD;
			utf8      = utf8.substr(result);
			if ((codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint >= 0x11'0000)
			{
				utf32.push_back(0xFFFD);
				isInvalid = true;
			}
			else
			{
				utf32.push_back(codepoint);
			}
		}
		return utf32;
	}

	std::string UTF16ToUTF8(std::u16string_view utf16)
	{
		std::string utf8;
		utf8.reserve(utf16.size());
		bool isInvalid = false;
		while (!utf16.empty())
		{
			char32_t codepoint = 0xFFFD;
			int8_t   result    = UTF16DecodeCodepoint(utf16, codepoint);
			if (result < 0)
			{
				if (!isInvalid)
				{
					// FFFD in utf8
					utf8.push_back(0b11101111);
					utf8.push_back(0b10111111);
					utf8.push_back(0b10111101);
				}
				isInvalid = true;
				utf16     = utf16.substr(-result);
				continue;
			}

			isInvalid = codepoint == 0xFFFD;
			utf16     = utf16.substr(result);
			isInvalid = !UTF8EncodeCodepoint(utf8, codepoint);
		}
		return utf8;
	}

	std::u32string UTF16ToUTF32(std::u16string_view utf16)
	{
		std::u32string utf32;
		utf32.reserve(utf16.size());
		bool isInvalid = false;
		while (!utf16.empty())
		{
			char32_t codepoint = 0xFFFD;
			int8_t   result    = UTF16DecodeCodepoint(utf16, codepoint);
			if (result < 0)
			{
				if (!isInvalid)
					utf32.push_back(0xFFFD);
				isInvalid = true;
				utf16     = utf16.substr(-result);
				continue;
			}

			isInvalid = codepoint == 0xFFFD;
			utf16     = utf16.substr(result);
			if (codepoint >= 0x11'0000)
			{
				utf32.push_back(0xFFFD);
				isInvalid = true;
			}
			else
			{
				utf32.push_back(codepoint);
			}
		}
		return utf32;
	}

	std::string UTF32ToUTF8(std::u32string_view utf32)
	{
		std::string utf8;
		utf8.reserve(utf32.size());
		for (size_t i = 0; i < utf32.size(); ++i)
			UTF8EncodeCodepoint(utf8, utf32[i]);
		return utf8;
	}

	std::u16string UTF32ToUTF16(std::u32string_view utf32)
	{
		std::u16string utf16;
		utf16.reserve(utf32.size());
		for (size_t i = 0; i < utf32.size(); ++i)
			UTF16EncodeCodepoint(utf16, utf32[i]);
		return utf16;
	}

	size_t UTF8ToUTF16Length(std::string_view utf8)
	{
		size_t length    = 0;
		bool   isInvalid = false;
		while (!utf8.empty())
		{
			char32_t codepoint = 0xFFFD;
			int8_t   result    = UTF8DecodeCodepoint(utf8, codepoint);
			if (result < 0)
			{
				if (!isInvalid)
					++length;
				isInvalid = true;
				utf8      = utf8.substr(-result);
				continue;
			}

			isInvalid    = codepoint == 0xFFFD;
			utf8         = utf8.substr(result);
			int8_t delta = UTF16EncodeCodepointLength(codepoint);
			isInvalid    = delta < 0;
			length      += delta < 0 ? -delta : delta;
		}
		return length;
	}

	size_t UTF8ToUTF32Length(std::string_view utf8)
	{
		size_t length    = 0;
		bool   isInvalid = false;
		while (!utf8.empty())
		{
			char32_t codepoint = 0xFFFD;
			int8_t   result    = UTF8DecodeCodepoint(utf8, codepoint);
			if (result < 0)
			{
				if (!isInvalid)
					++length;
				isInvalid = true;
				utf8      = utf8.substr(-result);
				continue;
			}

			isInvalid = codepoint == 0xFFFD;
			utf8      = utf8.substr(result);
			isInvalid = (codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint >= 0x11'0000;
			++length;
		}
		return length;
	}

	size_t UTF16ToUTF8Length(std::u16string_view utf16)
	{
		size_t length    = 0;
		bool   isInvalid = false;
		while (!utf16.empty())
		{
			char32_t codepoint = 0xFFFD;
			int8_t   result    = UTF16DecodeCodepoint(utf16, codepoint);
			if (result < 0)
			{
				if (!isInvalid)
					length += 3;
				isInvalid = true;
				utf16     = utf16.substr(-result);
				continue;
			}

			isInvalid    = codepoint == 0xFFFD;
			utf16        = utf16.substr(result);
			int8_t delta = UTF8EncodeCodepointLength(codepoint);
			isInvalid    = delta < 0;
			length      += delta < 0 ? -delta : delta;
		}
		return length;
	}

	size_t UTF16ToUTF32Length(std::u16string_view utf16)
	{
		size_t length    = 0;
		bool   isInvalid = false;
		while (!utf16.empty())
		{
			char32_t codepoint = 0xFFFD;
			int8_t   result    = UTF16DecodeCodepoint(utf16, codepoint);
			if (result < 0)
			{
				if (!isInvalid)
					++length;
				isInvalid = true;
				utf16     = utf16.substr(-result);
				continue;
			}

			isInvalid = codepoint == 0xFFFD;
			utf16     = utf16.substr(result);
			isInvalid = codepoint >= 0x11'0000;
			++length;
		}
		return length;
	}

	size_t UTF32ToUTF8Length(std::u32string_view utf32)
	{
		size_t length = 0;
		for (size_t i = 0; i < utf32.size(); ++i)
			length += abs(UTF8EncodeCodepointLength(utf32[i]));
		return length;
	}

	size_t UTF32ToUTF16Length(std::u32string_view utf32)
	{
		size_t length = 0;
		for (size_t i = 0; i < utf32.size(); ++i)
			length += abs(UTF16EncodeCodepointLength(utf32[i]));
		return length;
	}

	char32_t UTF32ToLower(char32_t codepoint)
	{
		auto itr = std::lower_bound(s_CaseFolds, s_CaseFolds + sizeof(s_CaseFolds) / sizeof(*s_CaseFolds), codepoint, [](const CaseFold& fold, char32_t codepoint) -> bool { return fold.From < codepoint; });
		if (itr == s_CaseFolds + sizeof(s_CaseFolds) / sizeof(*s_CaseFolds))
			return codepoint;
		return itr->To;
	}
} // namespace UTF