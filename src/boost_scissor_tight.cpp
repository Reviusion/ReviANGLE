// Boost: Tight Scissor Rect
// Sets a tight scissor rectangle around each draw call's bounding box,
// telling the GPU to skip fragment processing for pixels outside the rect.
// This reduces fill rate on large-screen renders where most pixels are
// covered by a small sprite.

#include <windows.h>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef int          GLint;
typedef int          GLsizei;

using EnableFn  = void(WINAPI*)(GLenum);
using ScissorFn = void(WINAPI*)(GLint, GLint, GLsizei, GLsizei);

static EnableFn  s_enable  = nullptr;
static ScissorFn s_scissor = nullptr;
static bool g_active = false;

namespace boost_scissor_tight {
    void apply() {
        if (!Config::get().scissor_tight) return;

        s_enable  = (EnableFn)glproxy::resolve("glEnable");
        s_scissor = (ScissorFn)glproxy::resolve("glScissor");

        if (s_enable && s_scissor) {
            g_active = true;
            angle::log("scissor_tight: active (off by default, use with caution)");
        }
    }

    // Set scissor for a sprite at (x,y,w,h) in screen coords
    void setRect(int x, int y, int w, int h) {
        if (!g_active) return;
        s_enable(0x0C11);  // GL_SCISSOR_TEST
        s_scissor(x, y, w, h);
    }

    bool isActive() { return g_active; }
}
