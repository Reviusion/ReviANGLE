#pragma once
#include <windows.h>

// Resolver that caches function pointers from ANGLE's libGLESv2.dll.
// Cocos2d / GD load most GL entry points via wglGetProcAddress after context
// creation, so only a handful of GL 1.1 symbols are called directly via imports.
// We forward those to ANGLE by name.

namespace glproxy {
    // returns an ANGLE function pointer or nullptr
    void* resolve(const char* name);

    // init: called once after ANGLE is loaded
    void init();
}

// helper macro for generating a forwarding stub
#define GLP_FORWARD(ret, name, sig, args) \
    extern "C" __declspec(dllexport) ret WINAPI gl_##name sig { \
        using Fn = ret (WINAPI *) sig; \
        static Fn fn = (Fn)glproxy::resolve(#name); \
        if (!fn) return (ret)0; \
        return fn args; \
    }

#define GLP_FORWARD_VOID(name, sig, args) \
    extern "C" __declspec(dllexport) void WINAPI gl_##name sig { \
        using Fn = void (WINAPI *) sig; \
        static Fn fn = (Fn)glproxy::resolve(#name); \
        if (!fn) return; \
        fn args; \
    }
