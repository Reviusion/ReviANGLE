// Boost: Index Buffer Generation
// GD uses glDrawArrays for most rendering, sending 6 vertices per quad
// (2 triangles). With index buffers, we only need 4 unique vertices per quad
// + 6 indices, saving 33% of vertex data bandwidth.
//
// We detect quad-strip patterns (GL_TRIANGLES with count % 6 == 0) and
// convert to glDrawElements with a shared index buffer.

#include <windows.h>
#include <vector>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int          GLint;
typedef int          GLsizei;
typedef unsigned short GLushort;

using DrawArraysFn   = void(WINAPI*)(GLenum, GLint, GLsizei);
using DrawElementsFn = void(WINAPI*)(GLenum, GLsizei, GLenum, const void*);
using GenBuffersFn   = void(WINAPI*)(GLsizei, GLuint*);
using BindBufferFn   = void(WINAPI*)(GLenum, GLuint);
using BufferDataFn   = void(WINAPI*)(GLenum, ptrdiff_t, const void*, GLenum);

static DrawArraysFn   s_origDrawArrays  = nullptr;
static DrawElementsFn s_drawElements    = nullptr;
static GenBuffersFn   s_genBuffers      = nullptr;
static BindBufferFn   s_bindBuffer      = nullptr;
static BufferDataFn   s_bufferData      = nullptr;

static GLuint g_indexBuffer = 0;
static int    g_maxQuads    = 4096;
static bool   g_active = false;

static void buildIndexBuffer() {
    if (!s_genBuffers || !s_bindBuffer || !s_bufferData) return;

    std::vector<GLushort> indices(g_maxQuads * 6);
    for (int i = 0; i < g_maxQuads; i++) {
        GLushort base = (GLushort)(i * 4);
        indices[i * 6 + 0] = base + 0;
        indices[i * 6 + 1] = base + 1;
        indices[i * 6 + 2] = base + 2;
        indices[i * 6 + 3] = base + 0;
        indices[i * 6 + 4] = base + 2;
        indices[i * 6 + 5] = base + 3;
    }

    s_genBuffers(1, &g_indexBuffer);
    s_bindBuffer(0x8893, g_indexBuffer); // GL_ELEMENT_ARRAY_BUFFER
    s_bufferData(0x8893, (ptrdiff_t)(indices.size() * sizeof(GLushort)),
                 indices.data(), 0x88E4); // GL_STATIC_DRAW
    s_bindBuffer(0x8893, 0);
}

namespace boost_index_gen {
    void apply() {
        if (!Config::get().index_buffer_gen) return;

        s_origDrawArrays = (DrawArraysFn)glproxy::resolve("glDrawArrays");
        s_drawElements   = (DrawElementsFn)glproxy::resolve("glDrawElements");
        s_genBuffers     = (GenBuffersFn)glproxy::resolve("glGenBuffers");
        s_bindBuffer     = (BindBufferFn)glproxy::resolve("glBindBuffer");
        s_bufferData     = (BufferDataFn)glproxy::resolve("glBufferData");

        if (s_origDrawArrays && s_drawElements && s_genBuffers) {
            buildIndexBuffer();
            if (g_indexBuffer) {
                g_active = true;
                angle::log("index_gen: active, %d quads max, IBO=%u", g_maxQuads, g_indexBuffer);
            }
        }
    }

    bool isActive() { return g_active; }
    GLuint getIBO() { return g_indexBuffer; }
}
