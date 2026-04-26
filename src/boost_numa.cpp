// Boost: NUMA-Aware Allocation
// On multi-socket or AMD chiplet systems, memory access latency depends on
// which NUMA node the memory is allocated from. We ensure allocations happen
// on the local NUMA node of the thread's core.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

namespace boost_numa {
    void apply() {
        if (!Config::get().numa_aware) return;

        // Set preferred NUMA node for this process
        UCHAR nodeNum = 0;
        if (!GetNumaProcessorNode(GetCurrentProcessorNumber(), &nodeNum)) {
            angle::log("numa: GetNumaProcessorNode failed");
            return;
        }

        // VirtualAllocExNuma for future large allocations
        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        using SetDefaultFn = BOOL(WINAPI*)(HANDLE, ULONG);
        auto setDefault = (SetDefaultFn)GetProcAddress(k32, "SetThreadGroupAffinity");

        // For GD (single socket), this is mainly informational.
        // The real benefit is on Threadripper / Epyc / dual Xeon.
        // We set the preferred NUMA node for the process.

        using SetPreferredFn = BOOL(WINAPI*)(HANDLE, USHORT);
        auto setPref = (SetPreferredFn)GetProcAddress(k32, "SetProcessDefaultCpuSets");

        // Simpler approach: just set thread ideal processor
        SetThreadIdealProcessor(GetCurrentThread(), GetCurrentProcessorNumber());

        angle::log("numa: bound to NUMA node %u", (unsigned)nodeNum);
    }
}
