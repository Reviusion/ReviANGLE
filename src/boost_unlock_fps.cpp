// Boost: Unlock FPS Cap
//
// cocos2d-x's main loop in CCApplication::run() sleeps to maintain a target frame
// time stored in CCApplication::m_dAnimationInterval. By default this is 1/60.
// Even with VSync off, ANGLE NO_ERROR context, and our DXGI hooks, the cocos2d
// loop will still cap at whatever interval is set.
//
// Strategy: locate CCApplication::sharedApplication() and CCApplication::setAnimationInterval(double)
// in libcocos2d.dll, and call setAnimationInterval(1/1000) at startup. Re-apply
// every N frames from the swap path so any mod that resets it (e.g., a settings
// menu writing 1/60 back) gets defeated within ~1s.
//
// Result: cocos2d main loop runs as fast as GPU/CPU allow; our boost_frame_pacing
// then optionally caps it back to a stable target (e.g. 165 Hz for the user's monitor).

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

// Cocos2d-x 2.2.x exports (verified present in GD's libcocos2d.dll):
//   ?sharedApplication@CCApplication@cocos2d@@SAPEAV12@XZ
//   ?setAnimationInterval@CCApplication@cocos2d@@QEAAXN@Z   (UEAAXN on some builds)
static const char* kSharedAppExport_Q = "?sharedApplication@CCApplication@cocos2d@@SAPEAV12@XZ";
static const char* kSetIntervalExport_Q = "?setAnimationInterval@CCApplication@cocos2d@@QEAAXN@Z";
static const char* kSetIntervalExport_U = "?setAnimationInterval@CCApplication@cocos2d@@UEAAXN@Z";

using SharedAppFn = void* (*)();
using SetIntervalFn = void (*)(void*, double);

static SharedAppFn   g_sharedApp   = nullptr;
static SetIntervalFn g_setInterval = nullptr;
static void*         g_appInstance = nullptr;
static bool          g_active      = false;

// Target interval — small value so cocos2d loop never sleeps. frame_pacing
// (or vsync if it slips through) will hold the actual cap.
static constexpr double kTargetInterval = 1.0 / 1000.0;  // 1000 FPS upper bound

namespace boost_unlock_fps {

    void apply() {
        if (!Config::get().unlock_fps_cap) {
            angle::log("unlock_fps: disabled by config");
            return;
        }

        HMODULE cocos = GetModuleHandleA("libcocos2d.dll");
        if (!cocos) {
            angle::log("unlock_fps: libcocos2d.dll not loaded yet (will retry on first swap)");
            return;
        }

        g_sharedApp = (SharedAppFn)GetProcAddress(cocos, kSharedAppExport_Q);
        g_setInterval = (SetIntervalFn)GetProcAddress(cocos, kSetIntervalExport_Q);
        if (!g_setInterval) {
            // Some builds have it as virtual (UEAAXN) instead of public (QEAAXN).
            g_setInterval = (SetIntervalFn)GetProcAddress(cocos, kSetIntervalExport_U);
        }

        if (!g_sharedApp || !g_setInterval) {
            angle::log("unlock_fps: missing exports (sharedApp=%p setInterval=%p)",
                       (void*)g_sharedApp, (void*)g_setInterval);
            return;
        }

        g_appInstance = g_sharedApp();
        if (!g_appInstance) {
            angle::log("unlock_fps: sharedApplication() returned NULL");
            return;
        }

        g_setInterval(g_appInstance, kTargetInterval);
        g_active = true;
        angle::log("unlock_fps: applied (app=%p, interval=%.6f s = %d FPS cap)",
                   g_appInstance, kTargetInterval, (int)(1.0 / kTargetInterval));
    }

    // Called from wgl_wglSwapBuffers ~once per second to defend against any mod
    // that re-sets m_dAnimationInterval to a low cap (e.g. settings menu).
    void reapply() {
        if (!g_active) {
            // Lazy init path: try once more if libcocos2d wasn't loaded at apply() time.
            if (!Config::get().unlock_fps_cap) return;
            if (!g_setInterval || !g_sharedApp || !g_appInstance) {
                apply();
            }
            return;
        }
        if (g_setInterval && g_appInstance) {
            g_setInterval(g_appInstance, kTargetInterval);
        }
    }

    bool isActive() { return g_active; }
}
