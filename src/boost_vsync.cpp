// Boost: force no VSync
// Calls eglSwapInterval(0) to disable VSync in ANGLE, removing the 60 FPS cap.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

static bool g_applied = false;

namespace boost_vsync {

    void apply() {
        if (!Config::get().force_no_vsync) return;
        // ANGLE may not be initialized yet at DllMain time.
        // We defer and call tryApply() later (e.g. from wgl_wglMakeCurrent).
        g_applied = false;
    }

    void tryApply() {
        if (g_applied || !Config::get().force_no_vsync) return;
        auto& a = angle::state();
        if (!a.initialized || !a.eglSwapInterval || !a.display) return;

        if (a.eglSwapInterval(a.display, 0)) {
            angle::log("vsync: disabled (eglSwapInterval 0)");
            g_applied = true;
        }
    }
}
