#pragma once

#include <stdint.h>

#ifdef _MSC_VER
#include <intrin.h>
#define DoDebugBreak() __debugbreak()
#define COMPILER_MSVC
#define HAS_SUPPORT_FOR_INTRINSICS
#endif

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t  s64;
typedef int32_t  s32;
typedef int16_t  s16;
typedef int8_t   s8;

typedef double   f64;
typedef float    f32;

// Copy-paste from https://gist.github.com/andrewrk/ffb272748448174e6cdb4958dae9f3d8
// Defer macro/thing.

#define CONCAT_INTERNAL(x,y) x##y
#define CONCAT(x,y) CONCAT_INTERNAL(x,y)

template<typename T>
struct ExitScope {
    T lambda;
    ExitScope(T lambda):lambda(lambda){}
    ~ExitScope(){lambda();}
    ExitScope(const ExitScope&);
  private:
    ExitScope& operator =(const ExitScope&);
};
 
class ExitScopeHelp {
  public:
    template<typename T>
        ExitScope<T> operator+(T t){ return t;}
};
 
#define defer const auto& CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()

#define ArrayCount(arr) (sizeof(arr)/sizeof((arr)[0]))
#define Max(x, y) ((x) > (y) ? (x) : (y))
#define Min(x, y) ((x) < (y) ? (x) : (y))

#define Bit(x) (1 << (x))
#define Square(x) ((x)*(x))

#if defined(BUILD_DEBUG) || defined(BUILD_RELEASE)
#define ASSERTIONS_ENABLED
#endif

#ifdef ASSERTIONS_ENABLED
#define Assert(expr) if (expr) {} else { DoDebugBreak(); }
#else
#define Assert(expr)
#endif

// From https://www.bytesbeneath.com/p/the-arena-custom-memory-allocators
#define IsPowerOfTwo(x) ((x != 0) && ((x & (x - 1)) == 0))

#define Kilobytes(x) ((x)*1024LL)
#define Megabytes(x) (Kilobytes(x)*1024LL)
#define Gigabytes(x) (Megabytes(x)*1024LL)
#define Terabytes(x) (Gigabytes(x)*1024LL)
