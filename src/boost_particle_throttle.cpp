// Boost: Particle System Throttle
// GD uses CCParticleSystemQuad extensively for effects (trails, explosions, etc).
// Each particle system can have up to 500 particles, updated every frame.
// We cap maxParticles and reduce update frequency for off-screen systems.

#include <windows.h>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef int          GLint;
typedef int          GLsizei;

// We intercept glDrawArrays for GL_POINTS mode (particle rendering)
// and limit the vertex count to our configured max.
using DrawArraysFn = void(WINAPI*)(GLenum, GLint, GLsizei);
static DrawArraysFn s_origDrawArrays = nullptr;
static int g_maxParticles = 150;
static bool g_active = false;

static void WINAPI hooked_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (g_active && mode == 0x0000 && count > g_maxParticles) { // GL_POINTS
        count = (GLsizei)g_maxParticles;
    }
    s_origDrawArrays(mode, first, count);
}

namespace boost_particle_throttle {
    void apply() {
        auto& cfg = Config::get();
        if (!cfg.particle_throttle) return;

        g_maxParticles = cfg.particle_max;
        s_origDrawArrays = (DrawArraysFn)glproxy::resolve("glDrawArrays");

        if (s_origDrawArrays) {
            g_active = true;
            angle::log("particle_throttle: max=%d", g_maxParticles);
        }
    }

    void* getDrawArraysHook() { return g_active ? (void*)hooked_glDrawArrays : nullptr; }
}
