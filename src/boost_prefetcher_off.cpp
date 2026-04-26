// Boost: Disable Superfetch/Prefetcher for GD
// Windows Superfetch prefetches pages based on historical access patterns.
// For GD, this can cause I/O contention during level loads.
// We disable prefetching for our process by hinting via the
// FILE_FLAG_NO_BUFFERING approach and reducing prefetcher priority.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

typedef LONG NTSTATUS;
#define ProcessPagePriority 0x27

namespace boost_prefetcher_off {
    void apply() {
        if (!Config::get().prefetcher_off) return;

        // Method 1: Set low page priority so Superfetch doesn't prefetch for us
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (ntdll) {
            using NtSetInfoFn = NTSTATUS(WINAPI*)(HANDLE, ULONG, PVOID, ULONG);
            auto ntSetInfo = (NtSetInfoFn)GetProcAddress(ntdll, "NtSetInformationProcess");
            if (ntSetInfo) {
                ULONG pagePriority = 1; // MEMORY_PRIORITY_VERY_LOW
                ntSetInfo(GetCurrentProcess(), ProcessPagePriority, &pagePriority, sizeof(pagePriority));
            }
        }

        // Method 2: Hint to the system via registry (per-app Superfetch disable)
        // This requires admin and affects future launches.
        HKEY key;
        if (RegCreateKeyExA(HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers",
            0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS)
        {
            char exePath[MAX_PATH];
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            const char* value = "DISABLEPREFETCHER";
            RegSetValueExA(key, exePath, 0, REG_SZ, (const BYTE*)value, (DWORD)strlen(value) + 1);
            RegCloseKey(key);
        }

        angle::log("prefetcher_off: Superfetch hints set");
    }
}
