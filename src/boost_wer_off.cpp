// Boost: Disable Windows Error Reporting
// WER hooks into the exception handler chain and collects crash data.
// This adds overhead to exception dispatch. For GD with our proxy DLL,
// we disable WER to reduce exception handling latency.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

namespace boost_wer_off {
    void apply() {
        if (!Config::get().wer_disable) return;

        // Disable WER for this process
        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        if (k32) {
            using SetErrorModeFn = UINT(WINAPI*)(UINT);
            auto setMode = (SetErrorModeFn)GetProcAddress(k32, "SetErrorMode");
            if (setMode) {
                // SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX
                setMode(0x0001 | 0x0002 | 0x8000);
            }

            // WerAddExcludedApplication (Win7+)
            using WerSetFlagsFn = HRESULT(WINAPI*)(DWORD);
            HMODULE wer = LoadLibraryA("wer.dll");
            if (wer) {
                auto werSetFlags = (WerSetFlagsFn)GetProcAddress(wer, "WerSetFlags");
                if (werSetFlags) {
                    werSetFlags(0x0004); // WER_FAULT_REPORTING_FLAG_DISABLE_THREAD_SUSPENSION
                }
            }
        }

        angle::log("wer_off: Windows Error Reporting disabled for GD");
    }
}
