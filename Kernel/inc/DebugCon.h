#pragma once

#include <stddef.h>

void DebugCon_WriteChar(char c);
void DebugCon_WriteChars(const char* str, size_t count);
void DebugCon_WriteString(const char* str);
void DebugCon_WriteFormatted(const char* fmt, ...);