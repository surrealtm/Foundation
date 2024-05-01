#pragma once

#define NOMINMAX

#include <assert.h> // For assert
#include <math.h>   // For ceil, pow...
#include <stdio.h>  // For printf
#include <memory.h> // For memset, memcpy...

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned long      u32;
typedef unsigned long long u64;

typedef signed char      s8;
typedef signed short     s16;
typedef signed long      s32;
typedef signed long long s64;

typedef float  f32;
typedef double f64;

typedef bool b8;

#define null 0

typedef s64 Hardware_Time;

constexpr s64 MAX_S64 = 9223372036854775807LL;
constexpr s64 MIN_S64 = 9223372036854775808LL; // This is a negative number, C++ is just shit!!
constexpr u64 MAX_U64 = 0xffffffffffffffff;
constexpr u64 MIN_U64 = 0;

constexpr s32 MAX_S32 =  2147483647;
constexpr s32 MIN_S32 = -2147483648;
constexpr u32 MAX_U32 =  0xffffffff;
constexpr u32 MIN_U32 =  0;

constexpr s16 MAX_S16 =  32767;
constexpr s16 MIN_S16 = -32768;
constexpr u16 MAX_U16 =  0xffff;
constexpr u16 MIN_U16 =  0;

constexpr s8 MAX_S8 =  127;
constexpr s8 MIN_S8 = -128;
constexpr u8 MAX_U8 =  0xff;
constexpr u8 MIN_U8 =  0;

constexpr f64 MAX_F64 =  1.7976931348623157e308L;
constexpr f64 MIN_F64 = -1.7976931348623157e308L;

constexpr f32 MAX_F32 =  3.40282347e38F;
constexpr f32 MIN_F32 = -3.40282347e38F;

constexpr f32 F32_EPSILON = 1e-5f;
constexpr f64 F64_EPSILON = 1e-8f;

#if FOUNDATION_WIN32
# define PRIu64 "llu"
# define PRId64 "lld"
# define PRIx64 "llx"
# define LITTLE_ENDIAN true
#else
# error "This platform is not supported."
#endif

static_assert(sizeof(b8)  == 1,                     "Invalid size for b8.");
static_assert(sizeof(u8)  == 1 && sizeof(s8)  == 1, "Invalid size for u8 / s8.");
static_assert(sizeof(u16) == 2 && sizeof(s16) == 2, "Invalid size for u16 / s16.");
static_assert(sizeof(u32) == 4 && sizeof(s32) == 4, "Invalid size for u32 / s32.");
static_assert(sizeof(u64) == 8 && sizeof(s64) == 8, "Invalid size for u64 / s64.");
static_assert(sizeof(f32) == 4 && sizeof(f64) == 8, "Invalid size for f32 / f64.");



/* ---------------------------------------------- Macro Helpers ---------------------------------------------- */

#define __INTERNAL_STRINGIFY(EXP) #EXP
#define STRINGIFY(EXP) __INTERNAL_STRINGIFY(EXP)

#define __INTERNAL_CONCAT(x,y) x##y
#define CONCAT(x,y) __INTERNAL_CONCAT(x,y)

#define ARRAY_SIZE(_array) (sizeof(_array) / sizeof((_array)[0]))

#define foundation_error(format, ...) os_write_to_console(__FILE__ "," STRINGIFY(__LINE__) ": " format, __VA_ARGS__)

#define align_to(value, alignment, type) ((type) (ceil((f64) (value) / (f64) (alignment)) * (alignment)))

#define min(lhs, rhs) ((lhs) < (rhs) ? (lhs) : (rhs))
#define max(lhs, rhs) ((lhs) > (rhs) ? (lhs) : (rhs))

#define clamp(value, min, max) (((value) < (min)) ? (min) : ((value) > (max) ? (max) : value))

#define BITWISE(T)                                                      \
    inline T  operator| (T a, T b)  { return (T)((int) a | (int) b); }; \
    inline T  operator& (T a, T b)  { return (T)((int) a & (int) b); }; \
    inline T  operator^ (T a, T b)  { return (T)((int) a ^ (int) b); }; \
    inline T  operator~ (T a)       { return (T)(~((int) a)); };        \
    inline T &operator|=(T &a, T b) { a = a | b; return a; };           \
    inline T &operator&=(T &a, T b) { a = a & b; return a; };           \
    inline T &operator^=(T &a, T b) { a = a ^ b; return a; };   



/* ----------------------------------------------- Defer Helper ----------------------------------------------- */

template<typename T>
struct ExitScope {
    T lambda;
    ExitScope(T lambda):lambda(lambda){}
    ~ExitScope(){lambda();}
};
 
class ExitScopeHelp {
  public:
    template<typename T>
    ExitScope<T> operator+(T t){ return t;}
};
 
#define defer const auto& CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()



/* ---------------------------------------------- Standard Units ---------------------------------------------- */

enum Time_Unit {
    Nanoseconds,
    Microseconds,
    Milliseconds,
    Seconds,
    Minutes,
};

const char *time_unit_suffix(Time_Unit unit);
