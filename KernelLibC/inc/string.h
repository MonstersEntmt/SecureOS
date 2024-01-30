#pragma once

#include "stddef.h"

bool isalnum(int ch);
bool isalpha(int ch);
bool islower(int ch);
bool isupper(int ch);
bool isdigit(int ch);
bool isxdigit(int ch);
bool iscntrl(int ch);
bool isgraph(int ch);
bool isspace(int ch);
bool isblank(int ch);
bool issprint(int ch);
bool ispunct(int ch);
int  tolower(int ch);
int  toupper(int ch);

char* strcpy(char* restrict dst, const char* restrict src);
char* strncpy(char* restrict dst, const char* restrict src, size_t count);
char* strcat(char* restrict dst, const char* restrict src);
char* strncat(char* restrict dst, const char* restrict src, size_t count);

size_t strlen(const char* str);
int    strcmp(const char* lhs, const char* rhs);
int    strncmp(const char* lhs, const char* rhs, size_t count);
char*  strchr(const char* str, int ch);
char*  strrchr(const char* str, int ch);
size_t strspn(const char* dst, const char* src);
size_t strcspn(const char* dst, const char* src);
char*  strpbrk(const char* dst, const char* breakset);
char*  strstr(const char* str, const char* substr);

void* memchr(const void* ptr, int ch, size_t count);
void* memrchr(const void* ptr, int ch, size_t count);
int   memcmp(const void* lhs, const void* rhs, size_t count);
void* memset(void* dst, int c, size_t count);
void* memcpy(void* dst, const void* src, size_t count);
void* memmove(void* dst, const void* src, size_t count);