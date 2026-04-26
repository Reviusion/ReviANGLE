// Boost: GL state deduplication
// Cocos2d-x frequently rebinds the same texture / program. Track current
// state per-thread and skip identical calls. Saves real GPU driver overhead.

#include <windows.h>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int          GLint;

static thread_local GLuint t_boundTex[32]  = {};   // per texture unit
static thread_local GLenum t_activeUnit    = 0x84C0;  // GL_TEXTURE0
static thread_local GLuint t_program       = 0;
static bool g_active = false;

using BindTexFn      = void (WINAPI*)(GLenum, GLuint);
using ActiveTexFn    = void (WINAPI*)(GLenum);
using UseProgramFn   = void (WINAPI*)(GLuint);

static BindTexFn    s_origBindTex  = nullptr;
static ActiveTexFn  s_origActiveTex = nullptr;
static UseProgramFn s_origUseProgram = nullptr;

static void WINAPI dedup_glBindTexture(GLenum target, GLuint tex) {
    if (g_active && target == 0x0DE1) {  // GL_TEXTURE_2D
        int unit = (int)(t_activeUnit - 0x84C0);
        if (unit >= 0 && unit < 32 && t_boundTex[unit] == tex) return;
        if (unit >= 0 && unit < 32) t_boundTex[unit] = tex;
    }
    s_origBindTex(target, tex);
}

static void WINAPI dedup_glActiveTexture(GLenum tex) {
    if (g_active && tex == t_activeUnit) return;
    t_activeUnit = tex;
    s_origActiveTex(tex);
}

static void WINAPI dedup_glUseProgram(GLuint prog) {
    if (g_active && prog == t_program) return;
    t_program = prog;
    s_origUseProgram(prog);
}

namespace boost_gl_dedup {

    void apply() {
        if (!Config::get().gl_state_dedup) return;

        s_origBindTex   = (BindTexFn)glproxy::resolve("glBindTexture");
        s_origActiveTex = (ActiveTexFn)glproxy::resolve("glActiveTexture");
        s_origUseProgram = (UseProgramFn)glproxy::resolve("glUseProgram");

        if (s_origBindTex && s_origActiveTex && s_origUseProgram) {
            g_active = true;
            angle::log("gl_dedup: active");
        }
    }

    void* getBindTexHook()    { return g_active ? (void*)dedup_glBindTexture : nullptr; }
    void* getActiveTexHook()  { return g_active ? (void*)dedup_glActiveTexture : nullptr; }
    void* getUseProgramHook() { return g_active ? (void*)dedup_glUseProgram : nullptr; }
}
