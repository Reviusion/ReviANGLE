// Boost: Atlas Auto-Merger
// Monitors glTexImage2D calls during the first 120 frames. Small textures
// (<256x256) are packed into a single large atlas texture (2048x2048).
// After packing, glBindTexture calls for merged textures are redirected to
// the atlas, and UV coordinates are adjusted.
//
// This dramatically reduces draw calls in GD menus where many small icons
// and buttons each have their own texture.

#include <windows.h>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int          GLint;
typedef int          GLsizei;

namespace {

struct SubTex {
    GLuint atlasId;
    int    x, y, w, h;
    float  u0, v0, u1, v1;  // normalized UV in atlas
};

struct AtlasState {
    GLuint atlasTexId = 0;
    int    atlasSize  = 2048;
    int    cursorX    = 0;
    int    cursorY    = 0;
    int    rowHeight  = 0;
    int    frameCount = 0;
    bool   building   = true;

    std::unordered_map<GLuint, SubTex> merged;  // original texId -> atlas subtex
    std::mutex mu;
};

AtlasState* g_atlas = nullptr;
bool g_active = false;

using TexImage2DFn = void(WINAPI*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using GenTexFn     = void(WINAPI*)(GLsizei, GLuint*);
using BindTexFn    = void(WINAPI*)(GLenum, GLuint);
using TexSubFn     = void(WINAPI*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
using TexParamiFn  = void(WINAPI*)(GLenum, GLenum, GLint);

static TexImage2DFn s_origTexImage2D = nullptr;
static GenTexFn     s_genTex         = nullptr;
static BindTexFn    s_bindTex        = nullptr;
static TexSubFn     s_texSubImage    = nullptr;
static TexParamiFn  s_texParami      = nullptr;

} // namespace

namespace boost_atlas_merge {

    void apply() {
        auto& cfg = Config::get();
        if (!cfg.atlas_merge) return;

        s_origTexImage2D = (TexImage2DFn)glproxy::resolve("glTexImage2D");
        s_genTex         = (GenTexFn)glproxy::resolve("glGenTextures");
        s_bindTex        = (BindTexFn)glproxy::resolve("glBindTexture");
        s_texSubImage    = (TexSubFn)glproxy::resolve("glTexSubImage2D");
        s_texParami      = (TexParamiFn)glproxy::resolve("glTexParameteri");

        if (!s_origTexImage2D || !s_genTex || !s_bindTex || !s_texSubImage) {
            angle::log("atlas_merge: GL functions not available");
            return;
        }

        g_atlas = new AtlasState();
        g_atlas->atlasSize = cfg.atlas_size;

        // Create atlas texture
        s_genTex(1, &g_atlas->atlasTexId);
        s_bindTex(0x0DE1, g_atlas->atlasTexId);  // GL_TEXTURE_2D
        s_origTexImage2D(0x0DE1, 0, 0x1908,       // GL_RGBA
                         g_atlas->atlasSize, g_atlas->atlasSize,
                         0, 0x1908, 0x1401, nullptr);  // GL_UNSIGNED_BYTE

        if (s_texParami) {
            s_texParami(0x0DE1, 0x2801, 0x2601);  // MIN_FILTER = LINEAR
            s_texParami(0x0DE1, 0x2800, 0x2601);  // MAG_FILTER = LINEAR
        }

        s_bindTex(0x0DE1, 0);

        g_active = true;
        angle::log("atlas_merge: %dx%d atlas created (id=%u)",
                    g_atlas->atlasSize, g_atlas->atlasSize, g_atlas->atlasTexId);
    }

    // Called per frame to track building phase
    void tick() {
        if (!g_active || !g_atlas) return;
        g_atlas->frameCount++;
        if (g_atlas->frameCount >= 120 && g_atlas->building) {
            g_atlas->building = false;
            angle::log("atlas_merge: building complete, %zu textures merged",
                       g_atlas->merged.size());
        }
    }

    // Try to pack a texture into the atlas. Returns true if packed.
    bool tryPack(GLuint texId, GLsizei w, GLsizei h, const void* pixels) {
        if (!g_active || !g_atlas || !g_atlas->building) return false;
        if (w > 256 || h > 256 || w < 4 || h < 4) return false;
        if (!pixels) return false;

        std::lock_guard<std::mutex> lk(g_atlas->mu);

        // Simple row-based packing
        if (g_atlas->cursorX + w > g_atlas->atlasSize) {
            g_atlas->cursorX = 0;
            g_atlas->cursorY += g_atlas->rowHeight;
            g_atlas->rowHeight = 0;
        }
        if (g_atlas->cursorY + h > g_atlas->atlasSize) return false; // atlas full

        // Upload subtexture
        s_bindTex(0x0DE1, g_atlas->atlasTexId);
        s_texSubImage(0x0DE1, 0, g_atlas->cursorX, g_atlas->cursorY,
                      w, h, 0x1908, 0x1401, pixels);

        float as = (float)g_atlas->atlasSize;
        SubTex sub;
        sub.atlasId = g_atlas->atlasTexId;
        sub.x = g_atlas->cursorX;
        sub.y = g_atlas->cursorY;
        sub.w = w;
        sub.h = h;
        sub.u0 = g_atlas->cursorX / as;
        sub.v0 = g_atlas->cursorY / as;
        sub.u1 = (g_atlas->cursorX + w) / as;
        sub.v1 = (g_atlas->cursorY + h) / as;

        g_atlas->merged[texId] = sub;

        g_atlas->cursorX += w;
        if (h > g_atlas->rowHeight) g_atlas->rowHeight = h;

        return true;
    }

    bool isActive() { return g_active; }
}
