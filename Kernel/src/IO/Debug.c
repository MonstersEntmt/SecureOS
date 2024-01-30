#include "IO/Debug.h"

#include <stdio.h>
#include <string.h>

static bool DebugOutWriteChar(struct __FILE* restrict stream, char c)
{
	DebugOutChar(c);
	return true;
}

static ssize_t DebugOutWriteChars(struct __FILE* restrict stream, const char* restrict str, size_t count)
{
	DebugOutChars(str, count);
	return count;
}

static bool DebugOutFlush(struct __FILE* restrict stream)
{
	return true;
}

static FILE DebugOutStream;

void DebugSetupRedirects(void)
{
	memset(&DebugOutStream, 0, sizeof(FILE));
	DebugOutStream.WriteChar  = &DebugOutWriteChar,
	DebugOutStream.WriteChars = &DebugOutWriteChars,
	DebugOutStream.Flush      = &DebugOutFlush,
	__FILE_REDIRECT_STDOUT(&DebugOutStream);
	__FILE_REDIRECT_STDERR(&DebugOutStream);
}

void DebugOutPrint(const char* restrict fmt, ...)
{
	va_list vlist;
	va_start(vlist, fmt);
	vfprintf(&DebugOutStream, fmt, vlist);
	va_end(vlist);
}