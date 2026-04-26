// Boost: Disable Spectre/Meltdown Mitigations
// Windows enables Spectre V2 (branch prediction) and Meltdown mitigations
// that add overhead to every syscall and context switch.
// For a single-player game, the security cost is unnecessary.
// We set the per-process mitigation policy to disable these.
//
// NOTE: This requires admin rights and affects security.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

// PROCESS_MITIGATION_POLICY enum values
#define ProcessSpeculativeControlPolicy 20

namespace boost_spectre_off {
    void apply() {
        if (!Config::get().spectre_off) return;

        // Try SetProcessMitigationPolicy (Win10+)
        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        if (!k32) return;

        using SetMitigationFn = BOOL(WINAPI*)(int, PVOID, SIZE_T);
        auto setMitigation = (SetMitigationFn)GetProcAddress(k32, "SetProcessMitigationPolicy");
        if (!setMitigation) {
            angle::log("spectre_off: SetProcessMitigationPolicy not found (pre-Win10?)");
            return;
        }

        // Disable speculative store bypass
        struct {
            union {
                DWORD Flags;
                struct {
                    DWORD SpeculativeStoreBypassDisable : 1;
                };
            };
        } ssb = {};
        ssb.SpeculativeStoreBypassDisable = 0; // disable the mitigation

        // This may fail if the policy is already locked by the OS
        BOOL ok = setMitigation(ProcessSpeculativeControlPolicy, &ssb, sizeof(ssb));
        if (ok) {
            angle::log("spectre_off: speculative control mitigations disabled");
        } else {
            angle::log("spectre_off: failed (err=%lu), policy may be locked", GetLastError());
        }
    }
}
