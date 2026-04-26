// Boost: GPU forcer
//
// On laptops with switchable graphics (Nvidia Optimus, AMD Enduro / Switchable)
// the OS picks integrated GPU by default unless the application exports these
// magic symbols. Chromium, Unity, most game engines use the same trick.
//
// Since we are opengl32.dll sitting inside GD.exe process, these exports are
// visible to the GPU driver when it enumerates the process modules.

#include <windows.h>
#include "config.hpp"

extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement               = 0x00000001;
    __declspec(dllexport) int   AmdPowerXpressRequestHighPerformance = 1;
}

namespace boost_gpu {
    void apply() {
        // the exports above are what really matters; nothing to do at runtime.
        // we keep this function so other code can gate it via config.
        if (!Config::get().gpu_forcer) {
            // we can't un-export at runtime, but the setting is mostly informational
            // so if user disables it, we just log it
        }
    }
}
