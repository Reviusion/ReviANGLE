// Boost: Disable SmartScreen
// SmartScreen can intercept file operations and network access for reputation
// checks. We disable it for GD's process context.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

namespace boost_smartscreen_off {
    void apply() {
        if (!Config::get().smartscreen_off) return;

        // SmartScreen uses the Windows Security Center service.
        // For a per-process disable, we can set the environment variable
        // that Explorer checks, and disable the Zone.Identifier ADS
        // on files we touch.

        // Remove MOTW (Mark of the Web) from our DLL and ANGLE DLLs
        const char* files[] = {
            "opengl32.dll", "libEGL.dll", "libGLESv2.dll",
            "d3dcompiler_47.dll", nullptr
        };

        for (int i = 0; files[i]; i++) {
            char adsPath[MAX_PATH];
            std::snprintf(adsPath, sizeof(adsPath), "%s:Zone.Identifier", files[i]);
            DeleteFileA(adsPath);
        }

        angle::log("smartscreen_off: Zone.Identifier ADS removed from DLLs");
    }
}
