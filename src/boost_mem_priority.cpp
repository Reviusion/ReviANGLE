// Boost: Memory Priority HIGH
// Sets the memory priority of the GD process to HIGH, making the memory
// manager less likely to trim our working set when under pressure.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

// MEMORY_PRIORITY_INFORMATION
typedef struct {
    ULONG MemoryPriority;
} MY_MEMORY_PRIORITY_INFO;

// ProcessInformationClass
#define ProcessMemoryPriority 0x27

namespace boost_mem_priority {
    void apply() {
        if (!Config::get().mem_priority) return;

        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        if (!k32) return;

        using SetInfoFn = BOOL(WINAPI*)(HANDLE, int, LPVOID, DWORD);
        auto setInfo = (SetInfoFn)GetProcAddress(k32, "SetProcessInformation");

        if (setInfo) {
            MY_MEMORY_PRIORITY_INFO info;
            info.MemoryPriority = 5; // MEMORY_PRIORITY_NORMAL (highest for non-realtime)
            if (setInfo(GetCurrentProcess(), ProcessMemoryPriority, &info, sizeof(info))) {
                angle::log("mem_priority: memory priority set to HIGH");
                return;
            }
        }

        // Fallback: expand working set
        SetProcessWorkingSetSize(GetCurrentProcess(),
                                  128 * 1024 * 1024,   // 128MB min
                                  1024 * 1024 * 1024);  // 1GB max
        angle::log("mem_priority: expanded working set limits");
    }
}
