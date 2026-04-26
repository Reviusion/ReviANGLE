// Boost: disable Windows power throttling on the GD process
//
// Starting with Windows 10, the scheduler can throttle background-ish threads
// by running them at reduced frequency. We explicitly opt the GD process out.

#include <windows.h>
#include "config.hpp"

// PROCESS_POWER_THROTTLING_STATE is available via processthreadsapi.h on newer SDKs,
// but we declare it manually to keep the MinGW/old-SDK builds happy.
#ifndef PROCESS_POWER_THROTTLING_EXECUTION_SPEED
#define PROCESS_POWER_THROTTLING_EXECUTION_SPEED 0x1
#endif
#ifndef PROCESS_POWER_THROTTLING_CURRENT_VERSION
#define PROCESS_POWER_THROTTLING_CURRENT_VERSION 1
#endif

typedef struct _PROC_POWER_THROTTLING_STATE {
    ULONG Version;
    ULONG ControlMask;
    ULONG StateMask;
} PROC_POWER_THROTTLING_STATE;

namespace boost_power {

    void apply() {
        if (!Config::get().power_boost) return;

        using SetProcInfoFn = BOOL (WINAPI*)(HANDLE, int, PVOID, DWORD);
        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        if (!k32) return;

        auto fn = (SetProcInfoFn)GetProcAddress(k32, "SetProcessInformation");
        if (!fn) return;  // too old Windows, silently skip

        PROC_POWER_THROTTLING_STATE state = {};
        state.Version     = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        state.StateMask   = 0;  // 0 = disable throttling

        // ProcessPowerThrottling = 4
        fn(GetCurrentProcess(), 4, &state, sizeof(state));
    }
}
