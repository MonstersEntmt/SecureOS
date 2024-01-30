#pragma once

#include "stdarg.h"
#include "stddef.h"
#include "stdint.h"

#define __FILE_STATE_EOF   128
#define __FILE_STATE_ERROR 64

typedef struct __FILE
{
	// IO Callbacks
	bool (*WriteChar)(struct __FILE* restrict stream, char c);
	int (*ReadChar)(struct __FILE* restrict stream);
	ssize_t (*WriteChars)(struct __FILE* restrict stream, const char* restrict str, size_t count);
	ssize_t (*ReadChars)(struct __FILE* restrict stream, char* restrict str, size_t count);

	// Callbacks
	bool (*Seek)(struct __FILE* restrict stream, ssize_t offset, int origin);
	bool (*Flush)(struct __FILE* restrict stream);

	size_t  Position;
	uint8_t State;

	uint64_t Userdatas[8];
} FILE;

extern FILE* __stdin;
extern FILE* __stdout;
extern FILE* __stderr;

FILE* __FILE_REDIRECT_STDIN(FILE* restrict stream);
FILE* __FILE_REDIRECT_STDOUT(FILE* restrict stream);
FILE* __FILE_REDIRECT_STDERR(FILE* restrict stream);

void __FILE_READ_BUFFER(FILE* restrict stream, const void* buf, size_t size);
void __FILE_WRITE_BUFFER(FILE* restrict stream, void* buf, size_t size);
void __FILE_READ_WRITE_BUFFER(FILE* restrict stream, void* buf, size_t size);

#define stdin  ((FILE* restrict const) __stdin)
#define stdout ((FILE* restrict const) __stdout)
#define stderr ((FILE* restrict const) __stderr)

#define EOF      (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END (-1)

ssize_t ftell(FILE* restrict stream);
int     fseek(FILE* restrict stream, ssize_t offset, int origin);
void    frewind(FILE* restrict stream); // INFO(MarcasRealAccount): Original name rewind could be used elsewhere, so I'll be using frewind to follow the newer file based function names

int     fgetc(FILE* restrict stream);
char*   fgets(char* restrict str, size_t count, FILE* restrict stream);
ssize_t fread(void* restrict buf, size_t size, size_t count, FILE* restrict stream);
bool    fputc(int ch, FILE* restrict stream);
bool    fputs(const char* restrict str, FILE* restrict stream);
ssize_t fwrite(const void* restrict buf, size_t size, size_t count, FILE* restrict stream);

int  fflush(FILE* restrict stream);
bool feof(FILE* restrict stream);
bool ferror(FILE* restrict stream);

int   getc(void); // INFO(MarcasRealAccount): Original name getchar does not follow file based function names, so I'll be using getc() for stdin default
char* gets(char* restrict str, size_t count);
bool  putc(int ch); // INFO(MarcasRealAccount): Original name putchar does not follow file based function names, so I'll be using putc() for stdout default
bool  puts(const char* restrict str);

ssize_t scanf(const char* restrict fmt, ...);
ssize_t vscanf(const char* restrict fmt, va_list vlist);
ssize_t sscanf(const char* restrict buf, const char* restrict fmt, ...);
ssize_t vsscanf(const char* restrict buf, const char* restrict fmt, va_list vlist);
ssize_t snscanf(const char* restrict buf, size_t bufSize, const char* restrict fmt, ...);
ssize_t vsnscanf(const char* restrict buf, size_t bufSize, const char* restrict fmt, va_list vlist);
ssize_t fscanf(FILE* restrict stream, const char* restrict fmt, ...);
ssize_t vfscanf(FILE* restrict stream, const char* restrict fmt, va_list vlist);

ssize_t printf(const char* restrict fmt, ...);
ssize_t vprintf(const char* restrict fmt, va_list vlist);
ssize_t sprintf(char* restrict buf, const char* restrict fmt, ...);
ssize_t vsprintf(char* restrict buf, const char* restrict fmt, va_list vlist);
ssize_t snprintf(char* restrict buf, size_t bufSize, const char* restrict fmt, ...);
ssize_t vsnprintf(char* restrict buf, size_t bufSize, const char* restrict fmt, va_list vlist);
ssize_t fprintf(FILE* restrict stream, const char* restrict fmt, ...);
ssize_t vfprintf(FILE* restrict stream, const char* restrict fmt, va_list vlist);