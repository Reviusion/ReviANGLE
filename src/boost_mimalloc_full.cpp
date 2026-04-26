// Boost: full mimalloc allocator
// Microsoft's mimalloc is 2-5x faster than the default CRT heap for small
// allocations (<256 bytes), which dominate in cocos2d-x (CCObject, CCNode, etc.)
//
// Two modes:
// 1. If mimalloc.dll is present in GD folder → LoadLibrary + redirect
// 2. Otherwise, fall back to the slab allocator in boost_allocator.cpp
//
// To use mode 1: download mimalloc-override.dll (x86) from
// https://github.com/microsoft/mimalloc/releases and rename to mimalloc.dll.

#include <windows.h>
#include "config.hpp"
#include "common/iat_hook.hpp"
#include "angle_loader.hpp"

using MallocFn   = void*(*)(size_t);
using FreeFn     = void(*)(void*);
using ReallocFn  = void*(*)(void*, size_t);
using CallocFn   = void*(*)(size_t, size_t);

static MallocFn  s_mi_malloc  = nullptr;
static FreeFn    s_mi_free    = nullptr;
static ReallocFn s_mi_realloc = nullptr;
static CallocFn  s_mi_calloc  = nullptr;

static MallocFn  s_origMalloc  = nullptr;
static FreeFn    s_origFree    = nullptr;
static ReallocFn s_origRealloc = nullptr;

static void* hooked_malloc(size_t n)          { return s_mi_malloc(n); }
static void  hooked_free(void* p)             { s_mi_free(p); }
static void* hooked_realloc(void* p, size_t n){ return s_mi_realloc(p, n); }

namespace boost_mimalloc_full {

    void apply() {
        if (!Config::get().mimalloc_full) return;

        HMODULE mi = LoadLibraryA("mimalloc.dll");
        if (!mi) {
            mi = LoadLibraryA("mimalloc-override.dll");
        }
        if (!mi) {
            angle::log("mimalloc: DLL not found, using default allocator");
            return;
        }

        s_mi_malloc  = (MallocFn)GetProcAddress(mi, "mi_malloc");
        s_mi_free    = (FreeFn)GetProcAddress(mi, "mi_free");
        s_mi_realloc = (ReallocFn)GetProcAddress(mi, "mi_realloc");
        s_mi_calloc  = (CallocFn)GetProcAddress(mi, "mi_calloc");

        if (!s_mi_malloc || !s_mi_free || !s_mi_realloc) {
            angle::log("mimalloc: exports not found in DLL");
            FreeLibrary(mi);
            return;
        }

        // hook CRT malloc/free in GD.exe
        const char* crtDlls[] = {"msvcrt.dll", "ucrtbase.dll", "api-ms-win-crt-heap-l1-1-0.dll"};
        for (auto* dll : crtDlls) {
            auto* origM = iat::hookInMainExe(dll, "malloc", (void*)hooked_malloc);
            auto* origF = iat::hookInMainExe(dll, "free", (void*)hooked_free);
            iat::hookInMainExe(dll, "realloc", (void*)hooked_realloc);
            if (origM && !s_origMalloc) s_origMalloc = (MallocFn)origM;
            if (origF && !s_origFree)   s_origFree   = (FreeFn)origF;
        }

        angle::log("mimalloc: active (mi_malloc=%p)", s_mi_malloc);
    }
}
