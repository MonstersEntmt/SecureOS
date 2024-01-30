#pragma once

#include <stddef.h>

void DebugSetupRedirects(void);
void DebugOutChar(char c);
void DebugOutChars(const char* restrict str, size_t size);
void DebugOutString(const char* restrict str);
void DebugOutPrint(const char* restrict fmt, ...);