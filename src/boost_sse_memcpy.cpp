// Boost: SSE2 memcpy/memset
// Replaces CRT memcpy/memset with SSE2-optimised versions using non-temporal
// stores for large transfers (>4KB). Avoids polluting CPU cache with vertex
// data that the GPU will consume.

#include <windows.h>
#include <emmintrin.h>
#include <cstring>
#include "config.hpp"
#include "common/iat_hook.hpp"
#include "angle_loader.hpp"

using MemcpyFn  = void*(*)(void*, const void*, size_t);
using MemsetFn  = void*(*)(void*, int, size_t);

static MemcpyFn s_origMemcpy = nullptr;
static MemsetFn s_origMemset = nullptr;

static void* sse_memcpy(void* dst, const void* src, size_t n) {
    if (n < 4096 || ((uintptr_t)dst & 15) || ((uintptr_t)src & 15)) {
        return s_origMemcpy ? s_origMemcpy(dst, src, n) : memcpy(dst, src, n);
    }

    auto* d = (__m128i*)dst;
    auto* s = (const __m128i*)src;
    size_t chunks = n / 64;

    for (size_t i = 0; i < chunks; i++) {
        __m128i r0 = _mm_load_si128(s + 0);
        __m128i r1 = _mm_load_si128(s + 1);
        __m128i r2 = _mm_load_si128(s + 2);
        __m128i r3 = _mm_load_si128(s + 3);
        _mm_stream_si128(d + 0, r0);
        _mm_stream_si128(d + 1, r1);
        _mm_stream_si128(d + 2, r2);
        _mm_stream_si128(d + 3, r3);
        d += 4; s += 4;
    }
    _mm_sfence();

    size_t rem = n - chunks * 64;
    if (rem > 0) std::memcpy(d, s, rem);

    return dst;
}

static void* sse_memset(void* dst, int val, size_t n) {
    if (n < 4096 || ((uintptr_t)dst & 15)) {
        return s_origMemset ? s_origMemset(dst, val, n) : memset(dst, val, n);
    }

    __m128i v = _mm_set1_epi8((char)val);
    auto* d = (__m128i*)dst;
    size_t chunks = n / 64;

    for (size_t i = 0; i < chunks; i++) {
        _mm_stream_si128(d + 0, v);
        _mm_stream_si128(d + 1, v);
        _mm_stream_si128(d + 2, v);
        _mm_stream_si128(d + 3, v);
        d += 4;
    }
    _mm_sfence();

    size_t rem = n - chunks * 64;
    if (rem > 0) std::memset(d, val, rem);

    return dst;
}

namespace boost_sse_memcpy {

    void apply() {
        if (!Config::get().sse_memcpy) return;

        // hook in the main exe
        s_origMemcpy = (MemcpyFn)iat::hookInMainExe("msvcrt.dll", "memcpy", (void*)sse_memcpy);
        s_origMemset = (MemsetFn)iat::hookInMainExe("msvcrt.dll", "memset", (void*)sse_memset);

        // also try ucrtbase (newer MSVC)
        if (!s_origMemcpy) {
            s_origMemcpy = (MemcpyFn)iat::hookInMainExe("ucrtbase.dll", "memcpy", (void*)sse_memcpy);
        }
        if (!s_origMemset) {
            s_origMemset = (MemsetFn)iat::hookInMainExe("ucrtbase.dll", "memset", (void*)sse_memset);
        }

        angle::log("sse_memcpy: hooked (memcpy=%p, memset=%p)", s_origMemcpy, s_origMemset);
    }
}
