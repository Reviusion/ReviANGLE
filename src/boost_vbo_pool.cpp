// Boost: VBO pool
// Cocos2d re-uploads vertex data every frame with glBufferData. This forces the
// driver to allocate + orphan buffers constantly. We provide a persistent ring
// buffer VBO and redirect glBufferData → glBufferSubData into it.

#include <windows.h>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "common/ring_buffer.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef int          GLint;
typedef int          GLsizei;
typedef int          GLsizeiptr;
typedef unsigned int GLuint;

constexpr GLenum GL_ARRAY_BUFFER = 0x8892;

using GenBuffersFn   = void(WINAPI*)(GLsizei, GLuint*);
using BindBufferFn   = void(WINAPI*)(GLenum, GLuint);
using BufferDataFn   = void(WINAPI*)(GLenum, GLsizeiptr, const void*, GLenum);
using BufferSubDataFn= void(WINAPI*)(GLenum, GLsizeiptr, GLsizeiptr, const void*);

static GenBuffersFn    s_genBuffers   = nullptr;
static BindBufferFn    s_bindBuffer   = nullptr;
static BufferDataFn    s_bufferData   = nullptr;
static BufferSubDataFn s_bufferSubData= nullptr;

static GLuint      g_poolVBO = 0;
static RingBuffer* g_ring    = nullptr;
static bool        g_active  = false;

namespace boost_vbo_pool {

    void initGL() {
        // called once after GL context is ready
        if (!Config::get().vbo_pool) return;

        s_genBuffers   = (GenBuffersFn)glproxy::resolve("glGenBuffers");
        s_bindBuffer   = (BindBufferFn)glproxy::resolve("glBindBuffer");
        s_bufferData   = (BufferDataFn)glproxy::resolve("glBufferData");
        s_bufferSubData= (BufferSubDataFn)glproxy::resolve("glBufferSubData");

        if (!s_genBuffers || !s_bindBuffer || !s_bufferData || !s_bufferSubData) return;

        size_t poolBytes = (size_t)Config::get().vbo_pool_size_mb * 1024 * 1024;
        g_ring = new RingBuffer(poolBytes);

        s_genBuffers(1, &g_poolVBO);
        s_bindBuffer(GL_ARRAY_BUFFER, g_poolVBO);
        s_bufferData(GL_ARRAY_BUFFER, (GLsizeiptr)poolBytes, nullptr, 0x88E0); // GL_STREAM_DRAW
        s_bindBuffer(GL_ARRAY_BUFFER, 0);

        g_active = true;
        angle::log("vbo_pool: %d MB pool VBO created (id=%u)", Config::get().vbo_pool_size_mb, g_poolVBO);
    }

    void apply() {
        // deferred until GL context is created (called from wgl_wglMakeCurrent)
    }

    bool isActive() { return g_active; }
    GLuint getPoolVBO() { return g_poolVBO; }

    // Returns offset into pool, or -1 on overflow.
    GLsizeiptr upload(GLenum target, GLsizeiptr size, const void* data) {
        if (!g_active || target != GL_ARRAY_BUFFER || !data || size <= 0) return -1;
        if ((size_t)size > g_ring->capacity() / 4) return -1;  // too big for pool

        size_t off = g_ring->alloc((size_t)size);
        if (off == SIZE_MAX) return -1;

        s_bindBuffer(GL_ARRAY_BUFFER, g_poolVBO);
        s_bufferSubData(GL_ARRAY_BUFFER, (GLsizeiptr)off, size, data);
        return (GLsizeiptr)off;
    }
}
