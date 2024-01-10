#pragma once

#include "stdarg.h"
#include "stddef.h"

typedef struct __FILE
{
	bool (*WriteChar)(struct __FILE* file, char c);
	int (*ReadChar)(struct __FILE* file);
	size_t (*WriteChars)(struct __FILE* file, const char* restrict str, size_t length);
	size_t (*ReadChars)(struct __FILE* file, char* restrict str, size_t length);
} FILE;

// size_t scanf(const char* restrict fmt, ...);
// size_t vscanf(const char* restrict fmt, va_list vlist);
// size_t sscanf(const char* restrict buffer, const char* restrict fmt, ...);
// size_t vsscanf(const char* restrict buffer, const char* restrict fmt, va_list vlist);
size_t fscanf(FILE* restrict stream, const char* restrict fmt, ...);
size_t vfscanf(FILE* restrict stream, const char* restrict fmt, va_list vlist);
size_t snscanf(const char* restrict buffer, size_t bufsize, const char* restrict fmt, ...);
size_t vsnscanf(const char* restrict buffer, size_t bufsize, const char* restrict fmt, va_list vlist);

// size_t printf(const char* restrict fmt, ...);
// size_t vprintf(const char* restrict fmt, va_list vlist);
// size_t sprintf(char* restrict buffer, const char* restrict fmt, ...);
// size_t vsprintf(char* restrict buffer, const char* restrict fmt, va_list vlist);
size_t fprintf(FILE* restrict stream, const char* restrict fmt, ...);
size_t vfprintf(FILE* restrict stream, const char* restrict fmt, va_list vlist);
size_t snprintf(char* restrict buffer, size_t bufsize, const char* restrict fmt, ...);
size_t vsnprintf(char* restrict buffer, size_t bufsize, const char* restrict fmt, va_list vlist);