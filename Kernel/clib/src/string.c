#include "string.h"
#include "stdint.h"

size_t strlen(const char* str)
{
	size_t len = 0;
	while (str[len] != '\0')
		++len;
	return len;
}

int strcmp(const char* lhs, const char* rhs)
{
	while (1)
	{
		char l = *lhs;
		char r = *rhs;
		if (l == '\0')
		{
			if (r == '\0')
				return 0;
			else
				return -1;
		}
		else if (r == '\0')
		{
			return 1;
		}
		if (l < r)
			return -1;
		else if (l > r)
			return 1;
		++lhs;
		++rhs;
	}
}

void* memmove(void* dst, const void* src, size_t count)
{
	void*       dstEnd = (uint8_t*) dst + count;
	const void* srcEnd = (const uint8_t*) src + count;
	if (dst <= srcEnd && dstEnd >= dst)
	{
		if (dst < src)
			return memcpy(dst, src, count);
		else
			return memcpy_reverse(dst, src, count);
	}
	else
	{
		return memcpy(dst, src, count);
	}
}