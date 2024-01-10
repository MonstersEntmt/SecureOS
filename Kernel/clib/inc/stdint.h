#pragma once

#define INT8_WIDTH        8
#define INT16_WIDTH       16
#define INT32_WIDTH       32
#define INT64_WIDTH       64
#define INT_FAST8_WIDTH   INT8_WIDTH
#define INT_FAST16_WIDTH  INT16_WIDTH
#define INT_FAST32_WIDTH  INT32_WIDTH
#define INT_FAST64_WIDTH  INT64_WIDTH
#define INT_LEAST8_WIDTH  INT8_WIDTH
#define INT_LEAST16_WIDTH INT16_WIDTH
#define INT_LEAST32_WIDTH INT32_WIDTH
#define INT_LEAST64_WIDTH INT64_WIDTH
#define INTMAX_WIDTH      INT64_WIDTH
#define INTPTR_WIDTH      INT64_WIDTH

#define INT8_MIN        0x80
#define INT16_MIN       0x8000
#define INT32_MIN       0x8000'0000
#define INT64_MIN       0x8000'0000'0000'0000L
#define INT_FAST8_MIN   INT8_MIN
#define INT_FAST16_MIN  INT16_MIN
#define INT_FAST32_MIN  INT32_MIN
#define INT_FAST64_MIN  INT64_MIN
#define INT_LEAST8_MIN  INT8_MIN
#define INT_LEAST16_MIN INT16_MIN
#define INT_LEAST32_MIN INT32_MIN
#define INT_LEAST64_MIN INT64_MIN
#define INTPTR_MIN      INT64_MIN
#define INTMAX_MIN      INT64_MIN

#define INT8_MAX        0x7F
#define INT16_MAX       0x7FFF
#define INT32_MAX       0x7FFF'FFFF
#define INT64_MAX       0x7FFF'FFFF'FFFF'FFFFL
#define INT_FAST8_MAX   INT8_MAX
#define INT_FAST16_MAX  INT16_MAX
#define INT_FAST32_MAX  INT32_MAX
#define INT_FAST64_MAX  INT64_MAX
#define INT_LEAST8_MAX  INT8_MAX
#define INT_LEAST16_MAX INT16_MAX
#define INT_LEAST32_MAX INT32_MAX
#define INT_LEAST64_MAX INT64_MAX
#define INTPTR_MAX      INT64_MAX
#define INTMAX_MAX      INT64_MAX

#define UINT8_WIDTH        INT8_WIDTH
#define UINT16_WIDTH       INT16_WIDTH
#define UINT32_WIDTH       INT32_WIDTH
#define UINT64_WIDTH       INT64_WIDTH
#define UINT_FAST8_WIDTH   INT_FAST8_WIDTH
#define UINT_FAST16_WIDTH  INT_FAST16_WIDTH
#define UINT_FAST32_WIDTH  INT_FAST32_WIDTH
#define UINT_FAST64_WIDTH  INT_FAST64_WIDTH
#define UINT_LEAST8_WIDTH  INT_LEAST8_WIDTH
#define UINT_LEAST16_WIDTH INT_LEAST16_WIDTH
#define UINT_LEAST32_WIDTH INT_LEAST32_WIDTH
#define UINT_LEAST64_WIDTH INT_LEAST64_WIDTH
#define UINTMAX_WIDTH      INTMAX_WIDTH
#define UINTPTR_WIDTH      INTPTR_WIDTH

#define UINT8_MIN        0U
#define UINT16_MIN       0U
#define UINT32_MIN       0U
#define UINT64_MIN       0U
#define UINT_FAST8_MIN   UINT8_MIN
#define UINT_FAST16_MIN  UINT16_MIN
#define UINT_FAST32_MIN  UINT32_MIN
#define UINT_FAST64_MIN  UINT64_MIN
#define UINT_LEAST8_MIN  UINT8_MIN
#define UINT_LEAST16_MIN UINT16_MIN
#define UINT_LEAST32_MIN UINT32_MIN
#define UINT_LEAST64_MIN UINT64_MIN
#define UINTPTR_MIN      UINT64_MIN
#define UINTMAX_MIN      UINT64_MIN

#define UINT8_MAX        0xFFU
#define UINT16_MAX       0xFFFFU
#define UINT32_MAX       0xFFFF'FFFFU
#define UINT64_MAX       0xFFFF'FFFF'FFFF'FFFFUL
#define UINT_FAST8_MAX   UINT8_MAX
#define UINT_FAST16_MAX  UINT16_MAX
#define UINT_FAST32_MAX  UINT32_MAX
#define UINT_FAST64_MAX  UINT64_MAX
#define UINT_LEAST8_MAX  UINT8_MAX
#define UINT_LEAST16_MAX UINT16_MAX
#define UINT_LEAST32_MAX UINT32_MAX
#define UINT_LEAST64_MAX UINT64_MAX
#define UINTPTR_MAX      UINT64_MAX
#define UINTMAX_MAX      UINT64_MAX

typedef signed char int8_t;
typedef short       int16_t;
typedef int         int32_t;
typedef long        int64_t; // Assuming long == 8 bytes
typedef int8_t      int_fast8_t;
typedef int16_t     int_fast16_t;
typedef int32_t     int_fast32_t;
typedef int64_t     int_fast64_t;
typedef int8_t      int_least8_t;
typedef int16_t     int_least16_t;
typedef int32_t     int_least32_t;
typedef int64_t     int_least64_t;
typedef int64_t     intmax_t;
typedef int64_t     intptr_t; // Assuming x86-64

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t; // Assuming long == 8 bytes
typedef uint8_t        uint_fast8_t;
typedef uint16_t       uint_fast16_t;
typedef uint32_t       uint_fast32_t;
typedef uint64_t       uint_fast64_t;
typedef uint8_t        uint_least8_t;
typedef uint16_t       uint_least16_t;
typedef uint32_t       uint_least32_t;
typedef uint64_t       uint_least64_t;
typedef uint64_t       uintmax_t;
typedef uint64_t       uintptr_t; // Assuming x86-64

#define INT8_C(x)    X
#define INT16_C(x)   X
#define INT32_C(x)   X
#define INT64_C(x)   x##L
#define INTMAX_C(x)  INT64_C(x)
#define UINT8_C(x)   X##U
#define UINT16_C(x)  X##U
#define UINT32_C(x)  X##U
#define UINT64_C(x)  x##UL
#define UINTMAX_C(x) UINT64_C(x)