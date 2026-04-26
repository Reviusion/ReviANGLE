// Boost: working set prefetch
// Pre-loads GD's code pages into physical RAM and locks critical pages.
// Reduces page faults during gameplay.

#include <windows.h>
#include <psapi.h>
#include "config.hpp"
#include "common/pe_utils.hpp"
#include "angle_loader.hpp"

// PrefetchVirtualMemory is Win8+ and not always in old SDK headers.
typedef BOOL (WINAPI *PrefetchVMFn)(HANDLE, ULONG_PTR, PVOID, ULONG);

namespace boost_prefetch {

    void apply() {
        if (!Config::get().working_set_prefetch) return;

        HMODULE gd = GetModuleHandleA(nullptr);
        BYTE*   codeStart = nullptr;
        SIZE_T  codeSize  = 0;

        if (!pe::codeRange(gd, &codeStart, &codeSize) || !codeStart) {
            angle::log("prefetch: couldn't find code section");
            return;
        }

        // Expand working set limits
        SetProcessWorkingSetSize(GetCurrentProcess(), 64 * 1024 * 1024, 512 * 1024 * 1024);

        // Try PrefetchVirtualMemory (Win8+)
        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        auto prefetch = (PrefetchVMFn)GetProcAddress(k32, "PrefetchVirtualMemory");
        if (prefetch) {
            WIN32_MEMORY_RANGE_ENTRY entry;
            entry.VirtualAddress = codeStart;
            entry.NumberOfBytes  = codeSize;
            prefetch(GetCurrentProcess(), 1, &entry, 0);
            angle::log("prefetch: prefetched %zu KB of code", codeSize / 1024);
        }

        // Lock the code pages (within working set quota)
        if (VirtualLock(codeStart, codeSize)) {
            angle::log("prefetch: locked code pages in RAM");
        } else {
            angle::log("prefetch: VirtualLock failed (quota?), prefetch-only");
        }
    }
}
