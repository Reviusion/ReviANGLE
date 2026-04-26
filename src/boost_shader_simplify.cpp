// Boost: Shader Simplify
// GD's fragment shaders have features not always needed (e.g. color tint
// multiply, alpha test threshold, multi-texturing). We intercept
// glShaderSource and replace complex shaders with simplified versions
// that skip unused branches.

#include <windows.h>
#include <cstring>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int          GLint;
typedef int          GLsizei;

using ShaderSourceFn = void(WINAPI*)(GLuint, GLsizei, const char**, const GLint*);
static ShaderSourceFn s_origShaderSource = nullptr;
static bool g_active = false;

// Simple optimized fragment shader — replaces GD's default
static const char* s_simpleFragSrc =
    "precision mediump float;\n"
    "varying vec2 v_texCoord;\n"
    "varying vec4 v_color;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_texture, v_texCoord) * v_color;\n"
    "}\n";

static void WINAPI hooked_glShaderSource(GLuint shader, GLsizei count,
    const char** strings, const GLint* lengths)
{
    if (g_active && count > 0 && strings && strings[0]) {
        // Detect GD's default fragment shader by looking for known patterns
        const char* src = strings[0];
        if (std::strstr(src, "CC_Texture0") || std::strstr(src, "u_texture")) {
            // Check if it's a simple textured shader we can optimize
            if (!std::strstr(src, "discard") && !std::strstr(src, "gl_FragDepth")) {
                // Safe to replace with simplified version
                const char* simplified = s_simpleFragSrc;
                s_origShaderSource(shader, 1, &simplified, nullptr);
                return;
            }
        }
    }
    s_origShaderSource(shader, count, strings, lengths);
}

namespace boost_shader_simplify {
    void apply() {
        if (!Config::get().shader_simplify) return;

        s_origShaderSource = (ShaderSourceFn)glproxy::resolve("glShaderSource");
        if (s_origShaderSource) {
            g_active = true;
            angle::log("shader_simplify: active — replacing complex fragment shaders");
        }
    }

    void* getShaderSourceHook() { return g_active ? (void*)hooked_glShaderSource : nullptr; }
}
