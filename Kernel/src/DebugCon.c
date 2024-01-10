#include "DebugCon.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void DebugCon_WriteString(const char* str)
{
	DebugCon_WriteChars(str, strlen(str));
}

static bool DebugConFileWriteChar(FILE* restrict file, char c)
{
	DebugCon_WriteChar(c);
	return true;
}

static size_t DebugConFileWriteChars(FILE* restrict file, const char* restrict str, size_t length)
{
	DebugCon_WriteChars(str, length);
	return length;
}

void DebugCon_WriteFormatted(const char* fmt, ...)
{
	struct __FILE debugConFile;
	debugConFile.WriteChar  = DebugConFileWriteChar;
	debugConFile.WriteChars = DebugConFileWriteChars;
	va_list vlist;
	va_start(vlist, fmt);
	vfprintf(&debugConFile, fmt, vlist);
	va_end(vlist);
}