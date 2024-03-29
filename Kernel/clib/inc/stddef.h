#pragma once

typedef unsigned long   size_t;     // Assuming unsigned long => 64 bit
typedef unsigned long   uptrdiff_t; // Assuming unsigned long => 64 bit
typedef long            ssize_t;    // Assuming long => 64 bit
typedef long            ptrdiff_t;  // Assuming long => 64 bit
typedef typeof(nullptr) nullptr_t;
typedef long double     max_align_t;

// #define nullptr        ((nullptr_t) 0)
#define NULL           nullptr
#define offsetof(s, m) __builtin_offsetof(s, m)

typedef int wint_t;