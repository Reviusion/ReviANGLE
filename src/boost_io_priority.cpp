// Boost: I/O Priority HIGH
// Sets the I/O priority of the GD process to HIGH, giving it precedence
// over background Windows services and indexers for disk access.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

// NtSetInformationProcess types
typedef LONG NTSTATUS;
#define ProcessIoPriority 33

namespace boost_io_priority {
    void apply() {
        if (!Config::get().io_priority) return;

        // Method 1: SetPriorityClass with ABOVE_NORMAL + background mode off
        HANDLE proc = GetCurrentProcess();
        SetPriorityClass(proc, ABOVE_NORMAL_PRIORITY_CLASS);

        // Method 2: NtSetInformationProcess for IO priority
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (ntdll) {
            using NtSetInfoFn = NTSTATUS(WINAPI*)(HANDLE, ULONG, PVOID, ULONG);
            auto ntSetInfo = (NtSetInfoFn)GetProcAddress(ntdll, "NtSetInformationProcess");
            if (ntSetInfo) {
                ULONG ioPriority = 3; // IoPriorityHigh
                NTSTATUS st = ntSetInfo(proc, ProcessIoPriority, &ioPriority, sizeof(ioPriority));
                if (st >= 0) {
                    angle::log("io_priority: I/O priority set to HIGH");
                    return;
                }
            }
        }

        angle::log("io_priority: using ABOVE_NORMAL priority class");
    }
}
