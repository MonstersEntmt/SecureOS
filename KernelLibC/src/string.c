#include "string.h"
#include "stdint.h"

extern void* memcpy_reverse(void* dst, const void* src, size_t count);

static const uint16_t s_CharsetLUT[128] = { 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x000D, 0x0005, 0x0005, 0x0005, 0x0005, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x000E, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0C52, 0x0C52, 0x0C52, 0x0C52, 0x0C52, 0x0C52, 0x0C52, 0x0C52, 0x0C52, 0x0C52, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x09D2, 0x09D2, 0x09D2, 0x09D2, 0x09D2, 0x09D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x01D2, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0032, 0x0AD2, 0x0AD2, 0x0AD2, 0x0AD2, 0x0AD2, 0x0AD2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x02D2, 0x0032, 0x0032, 0x0032, 0x0032, 0x0001 };
static const char     s_LowerLUT[128]   = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F };
static const char     s_UpperLUT[128]   = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F };

bool iscntrl(int ch)
{
	return ch < 0 || ch >= 128 ? false : (s_CharsetLUT[ch] & 1);
}

bool issprint(int ch)
{
	return ch < 0 || ch >= 128 ? false : ((s_CharsetLUT[ch] >> 1) & 1);
}

bool isspace(int ch)
{
	return ch < 0 || ch >= 128 ? false : ((s_CharsetLUT[ch] >> 2) & 1);
}

bool isblank(int ch)
{
	return ch < 0 || ch >= 128 ? false : ((s_CharsetLUT[ch] >> 3) & 1);
}

bool isgraph(int ch)
{
	return ch < 0 || ch >= 128 ? false : ((s_CharsetLUT[ch] >> 4) & 1);
}

bool ispunct(int ch)
{
	return ch < 0 || ch >= 128 ? false : ((s_CharsetLUT[ch] >> 5) & 1);
}

bool isalnum(int ch)
{
	return ch < 0 || ch >= 128 ? false : ((s_CharsetLUT[ch] >> 6) & 1);
}

bool isalpha(int ch)
{
	return ch < 0 || ch >= 128 ? false : ((s_CharsetLUT[ch] >> 7) & 1);
}

bool isupper(int ch)
{
	return ch < 0 || ch >= 128 ? false : ((s_CharsetLUT[ch] >> 8) & 1);
}

bool islower(int ch)
{
	return ch < 0 || ch >= 128 ? false : ((s_CharsetLUT[ch] >> 9) & 1);
}

bool isdigit(int ch)
{
	return ch < 0 || ch >= 128 ? false : ((s_CharsetLUT[ch] >> 10) & 1);
}

bool isxdigit(int ch)
{
	return ch < 0 || ch >= 128 ? false : ((s_CharsetLUT[ch] >> 11) & 1);
}

int tolower(int ch)
{
	return ch < 0 || ch >= 128 ? ch : s_LowerLUT[ch];
}

int toupper(int ch)
{
	return ch < 0 || ch >= 128 ? ch : s_UpperLUT[ch];
}

// TODO(MarcasRealAccount): Improve str functions

char* strcpy(char* restrict dst, const char* restrict src)
{
	memcpy(dst, src, strlen(src) + 1);
	return dst;
}

char* strncpy(char* restrict dst, const char* restrict src, size_t count)
{
	size_t toCopy = strlen(src);
	memcpy(dst, src, toCopy);
	if (count - toCopy > 0)
		memset(dst + toCopy, 0, count - toCopy);
	return dst;
}

char* strcat(char* restrict dst, const char* restrict src)
{
	size_t dstLen = strlen(dst);
	size_t srcLen = strlen(src);
	memcpy(dst + dstLen, src, srcLen + 1);
	return dst;
}

char* strncat(char* restrict dst, const char* restrict src, size_t count)
{
	size_t dstLen = strlen(dst);
	size_t srcLen = strlen(src);
	size_t toCopy = srcLen < count ? srcLen : count;
	memcpy(dst + dstLen, src, toCopy);
	dst[dstLen + toCopy] = '\0';
	return dst;
}

size_t strlen(const char* str)
{
	void* end = memchr(str, 0, ~0ULL);
	return (char*) end - str;
}

int strcmp(const char* lhs, const char* rhs)
{
	size_t  lhsLen = strlen(lhs);
	size_t  rhsLen = strlen(rhs);
	ssize_t diff   = lhsLen - rhsLen;
	if (diff != 0)
		return diff < 0 ? -1 : 1;
	return memcmp(lhs, rhs, lhsLen);
}

int strncmp(const char* lhs, const char* rhs, size_t count)
{
	size_t lhsLen = strlen(lhs);
	size_t rhsLen = strlen(rhs);
	lhsLen        = lhsLen < count ? lhsLen : count;
	rhsLen        = rhsLen < count ? rhsLen : count;
	ssize_t diff  = lhsLen - rhsLen;
	if (diff != 0)
		return diff < 0 ? -1 : 1;
	return memcmp(lhs, rhs, lhsLen);
}

char* strchr(const char* str, int ch)
{
	size_t len = strlen(str);
	return (char*) memchr(str, ch, len);
}

char* strrchr(const char* str, int ch)
{
	size_t len = strlen(str);
	return (char*) memrchr(str + len - 1, ch, len);
}

size_t strspn(const char* dst, const char* src)
{
	size_t srcLen = strlen(src);
	size_t offset = 0;
	while (true)
	{
		char c = dst[offset];
		for (size_t i = 0; i < srcLen; ++i)
		{
			if (src[i] == c)
			{
				++offset;
				goto CONTINUE;
			}
		}
		break;
	CONTINUE:
		continue;
	}
	return offset;
}

size_t strcspn(const char* dst, const char* src)
{
	size_t srcLen = strlen(src);
	size_t offset = 0;
	while (true)
	{
		char c = dst[offset];
		for (size_t i = 0; i < srcLen; ++i)
		{
			if (src[i] == c)
				goto BREAKOUT;
		}
		++offset;
		continue;
	BREAKOUT:
		break;
	}
	return offset;
}

char* strpbrk(const char* dst, const char* breakset)
{
	size_t breaksetLen = strlen(breakset);
	char*  cur         = (char*) dst;
	while (*cur)
	{
		char c = *cur;
		for (size_t i = 0; i < breaksetLen; ++i)
		{
			if (breakset[i] == c)
				goto BREAKOUT;
		}
		++cur;
		continue;
	BREAKOUT:
		break;
	}
	return cur;
}

char* strstr(const char* str, const char* substr)
{
	size_t substrLen = strlen(substr);
	char*  cur       = (char*) str;
	while (*cur)
	{
		for (size_t i = 0; i < substrLen; ++i)
		{
			if (cur[i] != substr[i])
				goto CONTINUE;
		}
		break;
	CONTINUE:
		++cur;
		continue;
	}
	return cur;
}

void* memchr(const void* ptr, int ch, size_t count)
{
	const uint8_t* pU = (const uint8_t*) ptr;
	for (size_t i = 0; i < count; ++i, ++pU)
	{
		if (*pU == (uint8_t) ch)
			break;
	}
	return (void*) pU;
}

void* memrchr(const void* ptr, int ch, size_t count)
{
	const uint8_t* pU = (const uint8_t*) ptr + count - 1;
	for (size_t i = 0; i < count; ++i, --pU)
	{
		if (*pU == (uint8_t) ch)
			break;
	}
	return (void*) pU;
}

void* memmove(void* dst, const void* src, size_t count)
{
	if (dst < src)
	{
		return memcpy(dst, src, count);
	}
	else
	{
		void*       dstEnd = (uint8_t*) dst + count - 1;
		const void* srcEnd = (const uint8_t*) src + count - 1;
		if (dst <= srcEnd && dstEnd >= dst)
			return memcpy_reverse((uint8_t*) dstEnd, (const uint8_t*) srcEnd, count);
		else
			return memcpy(dst, src, count);
	}
}