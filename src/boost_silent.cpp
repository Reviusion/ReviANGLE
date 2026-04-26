// Boost: silence debug output
// Hooks OutputDebugStringA and printf to no-op, saving CPU on debug builds
// of cocos2d and GD's own logging.

#include <windows.h>
#include <cstdio>
#include "config.hpp"
#include "common/iat_hook.hpp"
#include "angle_loader.hpp"

using OutputDebugStringAFn = void(WINAPI*)(LPCSTR);
using PrintfFn = int(*)(const char*, ...);

static OutputDebugStringAFn s_origODS = nullptr;

static void WINAPI noop_OutputDebugStringA(LPCSTR) {
    // intentionally empty
}

static int noop_printf(const char*, ...) {
    return 0;
}

namespace boost_silent {

    void apply() {
        if (!Config::get().silent_debug) return;

        s_origODS = (OutputDebugStringAFn)iat::hookInMainExe(
            "kernel32.dll", "OutputDebugStringA", (void*)noop_OutputDebugStringA);

        iat::hookInMainExe("msvcrt.dll", "printf", (void*)noop_printf);
        iat::hookInMainExe("ucrtbase.dll", "printf", (void*)noop_printf);

        angle::log("silent_debug: active");
    }
}
