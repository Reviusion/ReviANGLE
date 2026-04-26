// Boost: FBO Cache
// GD's ShaderLayer and other effects create/destroy FBOs every frame.
// glGenFramebuffers + glDeleteFramebuffers are surprisingly expensive on some
// drivers. We pool pre-allocated FBOs by size and reuse them.

#include <windows.h>
#include <vector>
#include <mutex>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int          GLsizei;

using GenFBOFn     = void(WINAPI*)(GLsizei, GLuint*);
using DelFBOFn     = void(WINAPI*)(GLsizei, const GLuint*);
using BindFBOFn    = void(WINAPI*)(GLenum, GLuint);

static GenFBOFn  s_origGenFBO  = nullptr;
static DelFBOFn  s_origDelFBO  = nullptr;
static BindFBOFn s_origBindFBO = nullptr;

namespace {

struct FBOPool {
    std::vector<GLuint> available;
    std::mutex mu;
    int maxSize = 8;
    int hits = 0;
    int misses = 0;

    GLuint acquire() {
        std::lock_guard<std::mutex> lk(mu);
        if (!available.empty()) {
            GLuint fbo = available.back();
            available.pop_back();
            hits++;
            return fbo;
        }
        misses++;
        GLuint fbo = 0;
        s_origGenFBO(1, &fbo);
        return fbo;
    }

    void release(GLuint fbo) {
        if (fbo == 0) return;
        std::lock_guard<std::mutex> lk(mu);
        if ((int)available.size() < maxSize) {
            available.push_back(fbo);
        } else {
            s_origDelFBO(1, &fbo);
        }
    }

    void preallocate(int n) {
        std::lock_guard<std::mutex> lk(mu);
        for (int i = 0; i < n; i++) {
            GLuint fbo = 0;
            s_origGenFBO(1, &fbo);
            if (fbo) available.push_back(fbo);
        }
    }
};

FBOPool* g_pool = nullptr;
bool g_active = false;

} // namespace

static void WINAPI hooked_glGenFramebuffers(GLsizei n, GLuint* ids) {
    if (g_active && n == 1) {
        *ids = g_pool->acquire();
        return;
    }
    s_origGenFBO(n, ids);
}

static void WINAPI hooked_glDeleteFramebuffers(GLsizei n, const GLuint* ids) {
    if (g_active && n == 1) {
        g_pool->release(*ids);
        return;
    }
    s_origDelFBO(n, ids);
}

namespace boost_fbo_cache {

    void apply() {
        auto& cfg = Config::get();
        if (!cfg.fbo_cache) return;

        s_origGenFBO  = (GenFBOFn)glproxy::resolve("glGenFramebuffers");
        s_origDelFBO  = (DelFBOFn)glproxy::resolve("glDeleteFramebuffers");
        s_origBindFBO = (BindFBOFn)glproxy::resolve("glBindFramebuffer");

        if (!s_origGenFBO || !s_origDelFBO) {
            angle::log("fbo_cache: GL functions not available");
            return;
        }

        g_pool = new FBOPool();
        g_pool->maxSize = cfg.fbo_pool_size;
        g_pool->preallocate(cfg.fbo_pool_size);

        g_active = true;
        angle::log("fbo_cache: %d FBOs pre-allocated", cfg.fbo_pool_size);
    }

    void* getGenHook() { return g_active ? (void*)hooked_glGenFramebuffers : nullptr; }
    void* getDelHook() { return g_active ? (void*)hooked_glDeleteFramebuffers : nullptr; }

    void shutdown() {
        if (g_pool) {
            angle::log("fbo_cache: hits=%d, misses=%d", g_pool->hits, g_pool->misses);
            delete g_pool;
            g_pool = nullptr;
        }
    }
}
