// Boost: Thread Stack Trim
// Default thread stack on Windows is 1MB. GD is 32-bit, so virtual address
// space is limited to 2-4GB. Each thread consumes 1MB of VA space for its stack.
// We hook CreateThread to set the stack size to 256KB for non-main threads,
// freeing ~750KB VA space per thread for textures/buffers.

#include <windows.h>
#include "config.hpp"
#include "common/iat_hook.hpp"
#include "angle_loader.hpp"

using CreateThreadFn = HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
static CreateThreadFn s_origCreateThread = nullptr;
static bool g_active = false;
static constexpr SIZE_T TRIMMED_STACK = 256 * 1024; // 256KB

static HANDLE WINAPI hooked_CreateThread(
    LPSECURITY_ATTRIBUTES lpAttr, SIZE_T stackSize,
    LPTHREAD_START_ROUTINE lpStart, LPVOID lpParam,
    DWORD dwFlags, LPDWORD lpThreadId)
{
    if (g_active && (stackSize == 0 || stackSize > TRIMMED_STACK)) {
        stackSize = TRIMMED_STACK;
    }
    return s_origCreateThread(lpAttr, stackSize, lpStart, lpParam, dwFlags, lpThreadId);
}

namespace boost_stack_trim {
    void apply() {
        if (!Config::get().stack_trim) return;

        s_origCreateThread = (CreateThreadFn)iat::hookInMainExe(
            "kernel32.dll", "CreateThread", (void*)hooked_CreateThread);

        if (s_origCreateThread) {
            g_active = true;
            angle::log("stack_trim: new threads capped at %zuKB stack", TRIMMED_STACK / 1024);
        }
    }
}
