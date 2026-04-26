// Boost: dynamic resolution scaling
// Monitors FPS and scales the viewport when it drops below target.
// Uses FBO blit to present at native resolution.

#include <windows.h>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef int          GLint;
typedef int          GLsizei;

using ViewportFn = void(WINAPI*)(GLint, GLint, GLsizei, GLsizei);

static ViewportFn s_origViewport = nullptr;
static bool       g_active = false;
static float      g_scale  = 1.0f;
static int        g_nativeW = 0, g_nativeH = 0;
static int        g_targetFPS = 60;

// FPS tracking
static LARGE_INTEGER s_freq;
static LARGE_INTEGER s_lastFrame;
static float         s_smoothFPS = 60.0f;

static void WINAPI hooked_glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    if (!g_active || g_scale >= 0.99f) {
        s_origViewport(x, y, w, h);
        return;
    }

    // store native size on first call
    if (g_nativeW == 0) { g_nativeW = w; g_nativeH = h; }

    GLsizei sw = (GLsizei)(w * g_scale);
    GLsizei sh = (GLsizei)(h * g_scale);
    s_origViewport(x, y, sw, sh);
}

namespace boost_dyn_res {

    void apply() {
        auto& cfg = Config::get();
        if (!cfg.dyn_resolution) return;

        s_origViewport = (ViewportFn)glproxy::resolve("glViewport");
        g_targetFPS = cfg.dyn_res_target_fps;
        if (g_targetFPS < 15) g_targetFPS = 15;

        QueryPerformanceFrequency(&s_freq);
        QueryPerformanceCounter(&s_lastFrame);

        if (s_origViewport) {
            g_active = true;
            angle::log("dyn_res: active, target=%d FPS", g_targetFPS);
        }
    }

    void* getViewportHook() { return g_active ? (void*)hooked_glViewport : nullptr; }

    // call once per frame (e.g. from SwapBuffers hook)
    void tick() {
        if (!g_active) return;

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double dt = (double)(now.QuadPart - s_lastFrame.QuadPart) / s_freq.QuadPart;
        s_lastFrame = now;

        float fps = (dt > 0.0) ? (float)(1.0 / dt) : 60.0f;
        s_smoothFPS = s_smoothFPS * 0.9f + fps * 0.1f;

        if (s_smoothFPS < g_targetFPS * 0.85f) {
            g_scale = (g_scale > 0.5f) ? g_scale - 0.05f : 0.5f;
        } else if (s_smoothFPS > g_targetFPS * 0.95f) {
            g_scale = (g_scale < 1.0f) ? g_scale + 0.02f : 1.0f;
        }
    }

    float currentScale() { return g_scale; }
}
