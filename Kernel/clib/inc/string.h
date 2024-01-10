#pragma once

#include "stddef.h"

size_t strlen(const char* str);
int    strcmp(const char* lhs, const char* rhs);

void* memset(void* dst, int c, size_t count);
void* memcpy(void* dst, const void* src, size_t count);
void* memcpy_reverse(void* dst, const void* src, size_t count);
void* memmove(void* dst, const void* src, size_t count);