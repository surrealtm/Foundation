#pragma once

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <memory.h>

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char      s8;
typedef signed short     s16;
typedef signed int       s32;
typedef signed long long s64;

typedef float  f32;
typedef double f64;

typedef unsigned char b8;

#define null 0

#if BUILD_WIN32
# define PRIu64 "lld"
# define PRIx64 "llx"
#else
# error "This platform is not supported."
#endif

static_assert(sizeof(u8)  == 1 && sizeof(s8)  == 1, "Invalid size for u8 / s8.");
static_assert(sizeof(u16) == 2 && sizeof(s16) == 2, "Invalid size for u16 / s16.");
static_assert(sizeof(u32) == 4 && sizeof(s32) == 4, "Invalid size for u32 / s32.");
static_assert(sizeof(u64) == 8 && sizeof(s64) == 8, "Invalid size for u64 / s64.");
static_assert(sizeof(f32) == 4 && sizeof(f64) == 8, "Invalid size for f32 / f64.");

#define __INTERNAL_STRINGIFY(EXP) #EXP
#define STRINGIFY(EXP) __INTERNAL_STRINGIFY(EXP)

#define report_error(format, ...) os_write_to_console(__FILE__ "," STRINGIFY(__LINE__) ": " format, __VA_ARGS__)

#define align_to(value, alignment, type) ((type) (ceil(value / (f64) alignment) * alignment))

#define min(lhs, rhs) ((lhs) < (rhs) ? (lhs) : (rhs))
#define max(lhs, rhs) ((lhs) > (rhs) ? (lhs) : (rhs))