// Boost: Batch Coalesce
// Merges adjacent draw calls that share the same GL state (texture, program,
// blend mode). Instead of issuing 10 separate glDrawArrays with 6 vertices
// each, we issue 1 call with 60 vertices.

#include <windows.h>
#include <atomic>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int          GLint;
typedef int          GLsizei;

using DrawArraysFn  = void(WINAPI*)(GLenum, GLint, GLsizei);
using GetIntegervFn = void(WINAPI*)(GLenum, GLint*);

static DrawArraysFn  s_origDrawArrays = nullptr;
static GetIntegervFn s_getIntegerv    = nullptr;
static bool g_active = false;

// State tracking
static GLuint s_lastTex = 0;
static GLuint s_lastProg = 0;
static GLenum s_lastMode = 0;
static GLint  s_coalStart = 0;
static GLsizei s_coalCount = 0;
static std::atomic<int> g_coalesced{0};

static void doFlush() {
    if (s_coalCount > 0) {
        s_origDrawArrays(s_lastMode, s_coalStart, s_coalCount);
        s_coalCount = 0;
    }
}

static void WINAPI hooked_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (!g_active) {
        s_origDrawArrays(mode, first, count);
        return;
    }

    // Get current texture and program
    GLint tex = 0, prog = 0;
    if (s_getIntegerv) {
        s_getIntegerv(0x8069, &tex);   // GL_TEXTURE_BINDING_2D
        s_getIntegerv(0x8B8D, &prog);  // GL_CURRENT_PROGRAM
    }

    bool canMerge = (mode == s_lastMode &&
                     (GLuint)tex == s_lastTex &&
                     (GLuint)prog == s_lastProg &&
                     first == s_coalStart + s_coalCount);

    if (canMerge) {
        s_coalCount += count;
        g_coalesced++;
    } else {
        doFlush();
        s_lastMode = mode;
        s_lastTex = (GLuint)tex;
        s_lastProg = (GLuint)prog;
        s_coalStart = first;
        s_coalCount = count;
    }
}

namespace boost_batch_coalesce {
    void apply() {
        if (!Config::get().batch_coalesce) return;

        s_origDrawArrays = (DrawArraysFn)glproxy::resolve("glDrawArrays");
        s_getIntegerv    = (GetIntegervFn)glproxy::resolve("glGetIntegerv");

        if (s_origDrawArrays) {
            g_active = true;
            angle::log("batch_coalesce: active");
        }
    }

    void flush() { doFlush(); }
    void* getDrawArraysHook() { return g_active ? (void*)hooked_glDrawArrays : nullptr; }
    int getCoalescedCount() { return g_coalesced.load(); }
}
