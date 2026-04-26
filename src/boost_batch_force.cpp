// Boost: Force SpriteBatchNode
// Cocos2d-x can batch sprites sharing the same atlas into a single draw call
// via CCSpriteBatchNode. GD doesn't always use this. We track bound textures
// across consecutive glDrawArrays(GL_TRIANGLES) calls and merge them when
// the same texture is bound.

#include <windows.h>
#include <vector>
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

// Track consecutive draws with same texture
static GLuint s_lastTexId = 0;
static GLint  s_pendingFirst = 0;
static GLsizei s_pendingCount = 0;

static void flushPending() {
    if (s_pendingCount > 0 && s_origDrawArrays) {
        s_origDrawArrays(0x0004, s_pendingFirst, s_pendingCount); // GL_TRIANGLES
        s_pendingCount = 0;
    }
}

static void WINAPI hooked_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (!g_active || mode != 0x0004) { // not GL_TRIANGLES
        flushPending();
        s_origDrawArrays(mode, first, count);
        return;
    }

    GLint texId = 0;
    if (s_getIntegerv) {
        s_getIntegerv(0x8069, &texId); // GL_TEXTURE_BINDING_2D
    }

    if ((GLuint)texId == s_lastTexId && s_pendingCount > 0 &&
        first == s_pendingFirst + s_pendingCount) {
        // Consecutive draw with same texture — merge
        s_pendingCount += count;
    } else {
        flushPending();
        s_lastTexId = (GLuint)texId;
        s_pendingFirst = first;
        s_pendingCount = count;
    }
}

namespace boost_batch_force {
    void apply() {
        if (!Config::get().batch_force) return;

        s_origDrawArrays = (DrawArraysFn)glproxy::resolve("glDrawArrays");
        s_getIntegerv    = (GetIntegervFn)glproxy::resolve("glGetIntegerv");

        if (s_origDrawArrays) {
            g_active = true;
            angle::log("batch_force: active");
        }
    }

    void flush() { flushPending(); }
    void* getDrawArraysHook() { return g_active ? (void*)hooked_glDrawArrays : nullptr; }
}
