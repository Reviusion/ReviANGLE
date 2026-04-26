// Boost: Present Skip (idle-frame elision)
//
// On idle scenes (main menu, dialog open, level pause), cocos2d still runs the
// full game loop at the configured framerate but issues zero new draw calls
// per frame. The frame's contents are bit-for-bit identical to the previous
// frame, yet the swap chain still presents - costing GPU power, fan ramp-up,
// and (on flip-model swap chains with allow-tearing) a CPU sync.
//
// This module:
//   - Tracks if any glDrawArrays / glDrawElements call happened in the
//     current frame (via the existing g_drawArrays / g_drawElements counters
//     in gl_proxy.cpp - exposed via gdangle_getDrawArraysCount / Elements).
//   - When wglSwapBuffers is about to commit a frame with 0 draws, optionally
//     skip the actual Present call. The driver keeps the previously-presented
//     contents on screen.
//
// Risks:
//   - Some game logic relies on Present-driven vsync timing for input
//     polling. If you skip too many Presents in a row, mouse/key events may
//     batch up and feel laggy. We cap consecutive skips at 4 to bound this.
//   - Some D3D11 modes (e.g. older drivers, certain swap chains) may flicker
//     if Present is skipped. Disabled by default.
//
// Opt-in via [BoostExtreme] present_skip_idle = true.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

extern "C" unsigned long long gdangle_getDrawArraysCount();
extern "C" unsigned long long gdangle_getDrawElementsCount();

namespace boost_present_skip {

static bool                s_enabled        = false;
static unsigned long long  s_lastDrawArr    = 0;
static unsigned long long  s_lastDrawElem   = 0;
static int                 s_consecutiveSkips = 0;
static const int           kMaxConsecutive  = 4;
static unsigned long long  s_skipped        = 0;
static unsigned long long  s_total          = 0;

void apply() {
    s_enabled = Config::get().present_skip_idle;
    if (s_enabled) {
        angle::log("present_skip: ENABLED (cap=%d consecutive skips)", kMaxConsecutive);
    }
}

// Returns true if wgl_wglSwapBuffers should SKIP the actual Present call.
// Called from wgl_proxy.cpp's swap path AFTER frame-pacing has decided this
// is a real present (i.e. not waiting on the timer).
extern "C" bool gdangle_shouldSkipPresent() {
    if (!s_enabled) return false;
    s_total++;

    unsigned long long da = gdangle_getDrawArraysCount();
    unsigned long long de = gdangle_getDrawElementsCount();
    bool hadDraws = (da != s_lastDrawArr) || (de != s_lastDrawElem);
    s_lastDrawArr = da; s_lastDrawElem = de;

    if (hadDraws) {
        s_consecutiveSkips = 0;
        return false;
    }
    if (s_consecutiveSkips >= kMaxConsecutive) {
        s_consecutiveSkips = 0;
        return false;
    }
    s_consecutiveSkips++;
    s_skipped++;
    return true;
}

extern "C" unsigned long long gdangle_getPresentSkipStats(unsigned long long* outTotal) {
    if (outTotal) *outTotal = s_total;
    return s_skipped;
}

} // namespace
