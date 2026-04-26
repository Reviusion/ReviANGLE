// Boost: Half-Resolution Effects
// GD's blur, glow, and ShaderLayer effects are rendered at full resolution.
// These are fill-rate heavy and bandwidth-intensive. We intercept FBO creation
// for effect passes and create them at half width/height, then upscale.

#include <windows.h>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int          GLint;
typedef int          GLsizei;

using TexImage2DFn  = void(WINAPI*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using ViewportFn    = void(WINAPI*)(GLint, GLint, GLsizei, GLsizei);

static TexImage2DFn s_origTexImage2D = nullptr;
static ViewportFn   s_origViewport   = nullptr;
static bool g_active = false;
static int g_fboDepth = 0; // track nested FBO renders

static void WINAPI hooked_glTexImage2D(GLenum target, GLint level, GLint fmt,
    GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void* data)
{
    // Detect FBO texture allocation (no data, RGBA, large size)
    if (g_active && data == nullptr && w >= 256 && h >= 256 &&
        format == 0x1908 && g_fboDepth > 0) { // GL_RGBA
        // Halve the resolution for effect passes
        w /= 2;
        h /= 2;
    }
    s_origTexImage2D(target, level, fmt, w, h, border, format, type, data);
}

namespace boost_halfres_fx {
    void apply() {
        if (!Config::get().halfres_effects) return;

        s_origTexImage2D = (TexImage2DFn)glproxy::resolve("glTexImage2D");
        s_origViewport   = (ViewportFn)glproxy::resolve("glViewport");

        if (s_origTexImage2D) {
            g_active = true;
            angle::log("halfres_fx: effects rendered at half resolution");
        }
    }

    void enterFBO() { if (g_active) g_fboDepth++; }
    void leaveFBO() { if (g_active && g_fboDepth > 0) g_fboDepth--; }

    void* getTexImage2DHook() { return g_active ? (void*)hooked_glTexImage2D : nullptr; }
}
