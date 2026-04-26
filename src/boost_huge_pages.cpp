// Boost: 2MB Huge/Large Pages
// Normal pages are 4KB. With thousands of texture allocations, the TLB
// (Translation Lookaside Buffer) thrashes. 2MB large pages reduce TLB misses
// by 512x per page, dramatically improving memory access latency.
//
// Requires SeLockMemoryPrivilege (admin must grant it).

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

namespace {

bool enableLockMemoryPrivilege() {
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
        return false;

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueA(nullptr, "SeLockMemoryPrivilege", &tp.Privileges[0].Luid)) {
        CloseHandle(token);
        return false;
    }

    BOOL ok = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    CloseHandle(token);
    return ok && GetLastError() == 0;
}

} // namespace

namespace boost_huge_pages {
    static SIZE_T g_largePageSize = 0;

    void apply() {
        if (!Config::get().huge_pages) return;

        if (!enableLockMemoryPrivilege()) {
            angle::log("huge_pages: SeLockMemoryPrivilege not available (need admin grant)");
            return;
        }

        g_largePageSize = GetLargePageMinimum();
        if (g_largePageSize == 0) {
            angle::log("huge_pages: large pages not supported by hardware");
            return;
        }

        angle::log("huge_pages: enabled, page size=%zuKB", g_largePageSize / 1024);
    }

    // Allocate memory using large pages
    void* allocLarge(size_t size) {
        if (g_largePageSize == 0) return nullptr;
        // Round up to large page boundary
        size = (size + g_largePageSize - 1) & ~(g_largePageSize - 1);
        return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
    }

    SIZE_T getPageSize() { return g_largePageSize; }
}
