// Boost: disable mipmap generation
// 2D sprites don't need mipmaps. Saves VRAM (~33% per texture) and GPU time
// spent generating mip levels.

#include <windows.h>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef int          GLint;

constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_LINEAR             = 0x2601;
constexpr GLenum GL_LINEAR_MIPMAP_LINEAR   = 0x2703;
constexpr GLenum GL_LINEAR_MIPMAP_NEAREST  = 0x2701;
constexpr GLenum GL_NEAREST_MIPMAP_LINEAR  = 0x2702;
constexpr GLenum GL_NEAREST_MIPMAP_NEAREST = 0x2700;

using GenMipmapFn  = void(WINAPI*)(GLenum);
using TexParamiFn  = void(WINAPI*)(GLenum, GLenum, GLint);

static GenMipmapFn  s_origGenMipmap  = nullptr;
static TexParamiFn  s_origTexParami  = nullptr;
static bool         g_active = false;

static void WINAPI hooked_glGenerateMipmap(GLenum) {
    // no-op: skip mipmap generation entirely
}

static void WINAPI hooked_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (g_active && pname == GL_TEXTURE_MIN_FILTER) {
        // convert any mipmap filter to plain linear
        if (param == (GLint)GL_LINEAR_MIPMAP_LINEAR ||
            param == (GLint)GL_LINEAR_MIPMAP_NEAREST ||
            param == (GLint)GL_NEAREST_MIPMAP_LINEAR ||
            param == (GLint)GL_NEAREST_MIPMAP_NEAREST)
        {
            param = (GLint)GL_LINEAR;
        }
    }
    s_origTexParami(target, pname, param);
}

namespace boost_mipmap_off {

    void apply() {
        if (!Config::get().mipmap_off) return;

        s_origGenMipmap = (GenMipmapFn)glproxy::resolve("glGenerateMipmap");
        s_origTexParami = (TexParamiFn)glproxy::resolve("glTexParameteri");

        if (s_origGenMipmap && s_origTexParami) {
            g_active = true;
            angle::log("mipmap_off: active");
        }
    }

    void* getGenMipmapHook()  { return g_active ? (void*)hooked_glGenerateMipmap : nullptr; }
    void* getTexParamiHook()  { return g_active ? (void*)hooked_glTexParameteri : nullptr; }
}
