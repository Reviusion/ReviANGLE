// Boost: smart CPU pinning
// On hybrid CPUs (Intel 12th gen+ with P-cores and E-cores), pins the main
// thread to a performance core. On older/uniform CPUs, does nothing.

#include <windows.h>
#include <vector>
#include "config.hpp"
#include "angle_loader.hpp"

namespace boost_smart_pin {

    void apply() {
        if (!Config::get().smart_cpu_pin) return;

        // Query logical processor info to find P-cores vs E-cores
        DWORD bufSize = 0;
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bufSize);
        if (bufSize == 0) return;

        std::vector<uint8_t> buf(bufSize);
        auto* info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf.data();
        if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &bufSize)) {
            angle::log("smart_pin: GetLogicalProcessorInformationEx failed");
            return;
        }

        DWORD_PTR pCoreMask = 0;
        DWORD_PTR eCoreMask = 0;
        int pCores = 0, eCores = 0;

        auto* ptr = (BYTE*)info;
        auto* end = ptr + bufSize;
        while (ptr < end) {
            auto* item = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
            if (item->Relationship == RelationProcessorCore) {
                DWORD_PTR mask = 0;
                for (WORD g = 0; g < item->Processor.GroupCount; g++) {
                    mask |= item->Processor.GroupMask[g].Mask;
                }
                // EfficiencyClass: 0 = E-core, 1 = P-core on Intel hybrid
                if (item->Processor.EfficiencyClass >= 1) {
                    pCoreMask |= mask;
                    pCores++;
                } else {
                    eCoreMask |= mask;
                    eCores++;
                }
            }
            ptr += item->Size;
        }

        if (pCores == 0 || pCoreMask == 0) {
            angle::log("smart_pin: no hybrid cores detected, skipping");
            return;
        }

        // pin main thread to P-cores
        SetThreadAffinityMask(GetCurrentThread(), pCoreMask);
        angle::log("smart_pin: main thread pinned to %d P-cores (mask=0x%lx), %d E-cores available",
                    pCores, (unsigned long)pCoreMask, eCores);
    }
}
