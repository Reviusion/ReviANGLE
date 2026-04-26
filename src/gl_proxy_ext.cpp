// GL 2.0+ / GLES 3.0 wrapper functions for mod compatibility
// These forward to ANGLE's libGLESv2.dll via glproxy::resolve

#include "gl_proxy.hpp"
#include "angle_loader.hpp"
#include <windows.h>
#include <cstddef>

// GL types
typedef unsigned int   GLenum;
typedef unsigned int   GLbitfield;
typedef unsigned int   GLuint;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned char  GLboolean;
typedef unsigned char  GLubyte;
typedef signed char    GLbyte;
typedef short          GLshort;
typedef unsigned short GLushort;
typedef float          GLfloat;
typedef double         GLdouble;
typedef char           GLchar;
typedef void           GLvoid;
typedef ptrdiff_t      GLintptr;
typedef ptrdiff_t      GLsizeiptr;

namespace glproxy {
    void* resolve(const char* name);
}

#define GLP_EXT_FORWARD(ret, name, sig, args) \
    typedef ret (WINAPI *PFN_##name)sig; \
    extern "C" __declspec(dllexport) ret WINAPI gl_gl##name sig { \
        static PFN_##name p = nullptr; \
        if (!p) p = (PFN_##name)glproxy::resolve("gl" #name); \
        return p ? p args : (ret)0; \
    }

#define GLP_EXT_FORWARD_VOID(name, sig, args) \
    typedef void (WINAPI *PFN_##name)sig; \
    extern "C" __declspec(dllexport) void WINAPI gl_gl##name sig { \
        static PFN_##name p = nullptr; \
        if (!p) p = (PFN_##name)glproxy::resolve("gl" #name); \
        if (p) p args; \
    }

// Read/Draw buffers
GLP_EXT_FORWARD_VOID(ReadBuffer, (GLenum src), (src))
GLP_EXT_FORWARD_VOID(DrawBuffers, (GLsizei n, const GLenum* bufs), (n, bufs))

// CopyTex
GLP_EXT_FORWARD_VOID(CopyTexImage2D, (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border), (target, level, internalformat, x, y, width, height, border))
GLP_EXT_FORWARD_VOID(CopyTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height), (target, level, xoffset, yoffset, x, y, width, height))

// GetTexParameter
GLP_EXT_FORWARD_VOID(GetTexParameteriv, (GLenum target, GLenum pname, GLint* params), (target, pname, params))
GLP_EXT_FORWARD_VOID(GetTexParameterfv, (GLenum target, GLenum pname, GLfloat* params), (target, pname, params))

// Buffers
GLP_EXT_FORWARD_VOID(GenBuffers, (GLsizei n, GLuint* buffers), (n, buffers))

// glBindBuffer dedup — cocos2d binds the same VBO/IBO on every sprite draw.
// Track per-target last-bound buffer; unknown targets fall through.
static int s_bufTargetSlot(GLenum target) {
    switch (target) {
        case 0x8892: return 0;  // GL_ARRAY_BUFFER
        case 0x8893: return 1;  // GL_ELEMENT_ARRAY_BUFFER
        case 0x88EB: return 2;  // GL_PIXEL_PACK_BUFFER
        case 0x88EC: return 3;  // GL_PIXEL_UNPACK_BUFFER
        case 0x8A11: return 4;  // GL_UNIFORM_BUFFER
        case 0x8C8E: return 5;  // GL_TRANSFORM_FEEDBACK_BUFFER
        case 0x8F36: return 6;  // GL_COPY_READ_BUFFER
        case 0x8F37: return 7;  // GL_COPY_WRITE_BUFFER
        default:     return -1; // pass through
    }
}
extern "C" void gdangle_invalidateVAPCache();  // forward decl
typedef void (WINAPI *PFN_BB)(GLenum, GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glBindBuffer(GLenum target, GLuint buffer) {
    static PFN_BB p = nullptr;
    if (!p) p = (PFN_BB)glproxy::resolve("glBindBuffer");
    int slot = s_bufTargetSlot(target);
    if (slot >= 0) {
        thread_local GLuint t_bound[8] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                                            0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
        if (t_bound[slot] == buffer) return;
        t_bound[slot] = buffer;
        // ARRAY_BUFFER change → VAP cache must be invalidated (pointer offsets
        // are interpreted relative to the bound array buffer).
        if (slot == 0) gdangle_invalidateVAPCache();
    }
    if (p) p(target, buffer);
}
// glBufferData / glBufferSubData — invalidate VAP cache for ARRAY_BUFFER (the
// buffer's contents changed, but layout state should also be re-checked since
// drivers may invalidate internal state on full BufferData uploads).
extern "C" void gdangle_invalidateVAPCache();
GLP_EXT_FORWARD_VOID(BufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage), (target, size, data, usage))
GLP_EXT_FORWARD_VOID(BufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size, const void* data), (target, offset, size, data))
GLP_EXT_FORWARD_VOID(DeleteBuffers, (GLsizei n, const GLuint* buffers), (n, buffers))
GLP_EXT_FORWARD(void*, MapBufferRange, (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access), (target, offset, length, access))
GLP_EXT_FORWARD(GLboolean, UnmapBuffer, (GLenum target), (target))
GLP_EXT_FORWARD(GLboolean, IsBuffer, (GLuint buffer), (buffer))
GLP_EXT_FORWARD_VOID(GetBufferParameteriv, (GLenum target, GLenum pname, GLint* params), (target, pname, params))

// VAO
GLP_EXT_FORWARD_VOID(GenVertexArrays, (GLsizei n, GLuint* arrays), (n, arrays))

// VAO bind dedup. Also clears VertexAttribArray bitmask cache because the
// enabled-attrib state is per-VAO and switches with the VAO.
thread_local unsigned int g_vaaEnabledMask = 0;  // bit i = attrib i enabled
thread_local GLuint        g_currentVAO     = 0xFFFFFFFFu;
typedef void (WINAPI *PFN_BVA)(GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glBindVertexArray(GLuint array) {
    static PFN_BVA p = nullptr;
    if (!p) p = (PFN_BVA)glproxy::resolve("glBindVertexArray");
    if (array == g_currentVAO) return;
    g_currentVAO = array;
    g_vaaEnabledMask = 0;          // VAA enabled-state is per-VAO
    gdangle_invalidateVAPCache();  // VertexAttribPointer state is also per-VAO
    if (p) p(array);
}
GLP_EXT_FORWARD_VOID(DeleteVertexArrays, (GLsizei n, const GLuint* arrays), (n, arrays))
GLP_EXT_FORWARD(GLboolean, IsVertexArray, (GLuint array), (array))

// Shaders — track type per ID for source patching
#include <unordered_map>
#include <string>
#include <vector>
#include <array>
#include <cstring>
static std::unordered_map<GLuint, GLenum> g_shaderType;

typedef GLuint (WINAPI *PFN_CS)(GLenum);
extern "C" __declspec(dllexport) GLuint WINAPI gl_glCreateShader(GLenum type) {
    static PFN_CS p = nullptr;
    if (!p) p = (PFN_CS)glproxy::resolve("glCreateShader");
    GLuint id = p ? p(type) : 0;
    if (id) g_shaderType[id] = type;
    return id;
}
GLP_EXT_FORWARD_VOID(DeleteShader, (GLuint shader), (shader))

// Patch shader source: add "#version 100\nprecision mediump float;\n" if missing.
// cocos2d-x 2.2 ships desktop GLSL 1.10 sources without precision qualifiers,
// which ANGLE's GLES translator rejects -> shader doesn't compile -> link fails.
typedef void (WINAPI *PFN_SS)(GLuint, GLsizei, const GLchar* const*, const GLint*);
extern "C" __declspec(dllexport) void WINAPI gl_glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    static PFN_SS p = nullptr;
    if (!p) p = (PFN_SS)glproxy::resolve("glShaderSource");
    if (!p) return;

    // Concatenate all input strings to inspect
    std::string src;
    for (GLsizei i = 0; i < count; i++) {
        if (!string[i]) continue;
        if (length && length[i] > 0) src.append(string[i], length[i]);
        else src.append(string[i]);
    }

    bool isFragment = false;
    auto it = g_shaderType.find(shader);
    if (it != g_shaderType.end() && it->second == 0x8B30 /*GL_FRAGMENT_SHADER*/) isFragment = true;

    bool hasVersion = src.find("#version") != std::string::npos;

    // ALWAYS prepend our header for safety. cocos2d-x source layouts vary;
    // our tactic: insert precision before user declarations even if some precision
    // exists later — extras are harmless redundant declarations.
    std::string prefix;
    if (!hasVersion) prefix += "#version 100\n";
    if (isFragment) prefix += "precision mediump float;\nprecision mediump int;\n";
    else            prefix += "precision mediump int;\n";

    std::string patched = prefix + src;
    const GLchar* ptrs[1] = { patched.c_str() };
    GLint lens[1] = { (GLint)patched.size() };
    p(shader, 1, ptrs, lens);

    static int n = 0;
    if (n < 8) angle::log("ShaderSource patch #%d shader=%u type=%s (prefix %zu B, src %zu B)",
                          n, shader, isFragment ? "FRAG" : "VERT", prefix.size(), src.size());
    n++;
}

typedef void (WINAPI *PFN_CMP)(GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glCompileShader(GLuint shader) {
    static PFN_CMP p = nullptr;
    if (!p) p = (PFN_CMP)glproxy::resolve("glCompileShader");
    if (p) p(shader);
    // Check status & log info log on failure
    typedef void (WINAPI *PFN_GSV)(GLuint, GLenum, GLint*);
    typedef void (WINAPI *PFN_GSIL)(GLuint, GLsizei, GLsizei*, char*);
    static PFN_GSV pgsv = (PFN_GSV)glproxy::resolve("glGetShaderiv");
    static PFN_GSIL pgsil = (PFN_GSIL)glproxy::resolve("glGetShaderInfoLog");
    static int n = 0;
    if (n < 16 && pgsv) {
        GLint cs = -1; pgsv(shader, 0x8B81 /*GL_COMPILE_STATUS*/, &cs);
        if (cs == 0) {
            char buf[512] = {};
            if (pgsil) pgsil(shader, 511, nullptr, buf);
            angle::log("CompileShader FAIL #%d shader=%u: %s", n, shader, buf);
        } else if (n < 4) {
            angle::log("CompileShader OK #%d shader=%u", n, shader);
        }
        n++;
    }
}
GLP_EXT_FORWARD_VOID(GetShaderiv, (GLuint shader, GLenum pname, GLint* params), (shader, pname, params))
GLP_EXT_FORWARD_VOID(GetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog), (shader, bufSize, length, infoLog))
GLP_EXT_FORWARD_VOID(GetShaderSource, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source), (shader, bufSize, length, source))
GLP_EXT_FORWARD(GLboolean, IsShader, (GLuint shader), (shader))

// Programs
GLP_EXT_FORWARD(GLuint, CreateProgram, (void), ())
GLP_EXT_FORWARD_VOID(DeleteProgram, (GLuint program), (program))
GLP_EXT_FORWARD_VOID(AttachShader, (GLuint program, GLuint shader), (program, shader))
GLP_EXT_FORWARD_VOID(DetachShader, (GLuint program, GLuint shader), (program, shader))
typedef void (WINAPI *PFN_LP)(GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glLinkProgram(GLuint program) {
    static PFN_LP p = nullptr;
    if (!p) p = (PFN_LP)glproxy::resolve("glLinkProgram");
    if (p) p(program);
    // Log link status immediately
    typedef void (WINAPI *PFN_GPV)(GLuint, GLenum, GLint*);
    static PFN_GPV pgpiv = nullptr;
    if (!pgpiv) pgpiv = (PFN_GPV)glproxy::resolve("glGetProgramiv");
    static int n = 0;
    if (n < 30 && pgpiv) {
        GLint ls = -1; pgpiv(program, 0x8B82 /*GL_LINK_STATUS*/, &ls);
        angle::log("glLinkProgram #%d: prog=%u link_status=%d", n, program, ls);
        if (ls == 0) {
            // log info log
            typedef void (WINAPI *PFN_GPIL)(GLuint, GLsizei, GLsizei*, char*);
            static PFN_GPIL pgpil = (PFN_GPIL)glproxy::resolve("glGetProgramInfoLog");
            if (pgpil) {
                char buf[512] = {}; GLsizei len = 0;
                pgpil(program, 511, &len, buf);
                angle::log("  link_log: %s", buf);
            }
        }
        n++;
    }
}

typedef void (WINAPI *PFN_UP)(GLuint);
// thread_local current program — used by uniform dedup downstream
thread_local GLuint g_currentProgram = 0xFFFFFFFFu;
extern "C" __declspec(dllexport) void WINAPI gl_glUseProgram(GLuint program) {
    static PFN_UP p = nullptr;
    if (!p) p = (PFN_UP)glproxy::resolve("glUseProgram");
    if (program == g_currentProgram) return;
    g_currentProgram = program;
    if (p) p(program);
}
GLP_EXT_FORWARD_VOID(GetProgramiv, (GLuint program, GLenum pname, GLint* params), (program, pname, params))
GLP_EXT_FORWARD_VOID(GetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog), (program, bufSize, length, infoLog))
GLP_EXT_FORWARD_VOID(ValidateProgram, (GLuint program), (program))
GLP_EXT_FORWARD(GLboolean, IsProgram, (GLuint program), (program))
GLP_EXT_FORWARD_VOID(BindAttribLocation, (GLuint program, GLuint index, const GLchar* name), (program, index, name))
GLP_EXT_FORWARD(GLint, GetAttribLocation, (GLuint program, const GLchar* name), (program, name))
GLP_EXT_FORWARD_VOID(GetActiveAttrib, (GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name), (program, index, bufSize, length, size, type, name))
GLP_EXT_FORWARD_VOID(GetActiveUniform, (GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name), (program, index, bufSize, length, size, type, name))

// Uniforms
GLP_EXT_FORWARD(GLint, GetUniformLocation, (GLuint program, const GLchar* name), (program, name))

// ===== Scalar uniform dedup =====
// cocos2d-x re-uploads identical color / alpha / sampler uniforms every sprite.
// Direct-mapped per-thread cache keyed by (program, location). Skips driver
// upload when value is unchanged. Real CPU win on draw-heavy frames.
//
// 32 entries × 5 variants × 1 thread ≈ 4 KB/thread overhead. Hash collisions
// just cause an unnecessary upload, never a wrong upload.

extern thread_local GLuint g_currentProgram;

template <typename T, int K>
struct UniCache {
    GLuint prog;
    GLint  loc;
    T      val[K];
};
static constexpr unsigned UN_HASH_N = 32;
static inline unsigned uniHash(GLuint prog, GLint loc) {
    return ((unsigned)prog * 2654435761u + (unsigned)loc) & (UN_HASH_N - 1);
}

typedef void (WINAPI *PFN_U1F)(GLint, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform1f(GLint location, GLfloat v0) {
    static PFN_U1F p = nullptr;
    if (!p) p = (PFN_U1F)glproxy::resolve("glUniform1f");
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 1> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == v0) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = v0;
    }
    if (p) p(location, v0);
}
// glUniform2f dedup.
typedef void (WINAPI *PFN_U2F)(GLint, GLfloat, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    static PFN_U2F p = nullptr;
    if (!p) p = (PFN_U2F)glproxy::resolve("glUniform2f");
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 2> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == v0 && e.val[1] == v1) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = v0; e.val[1] = v1;
    }
    if (p) p(location, v0, v1);
}
// glUniform3f dedup.
typedef void (WINAPI *PFN_U3F)(GLint, GLfloat, GLfloat, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    static PFN_U3F p = nullptr;
    if (!p) p = (PFN_U3F)glproxy::resolve("glUniform3f");
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 3> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == v0 && e.val[1] == v1 && e.val[2] == v2) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = v0; e.val[1] = v1; e.val[2] = v2;
    }
    if (p) p(location, v0, v1, v2);
}

typedef void (WINAPI *PFN_U4F)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    static PFN_U4F p = nullptr;
    if (!p) p = (PFN_U4F)glproxy::resolve("glUniform4f");
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 4> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == v0 && e.val[1] == v1 && e.val[2] == v2 && e.val[3] == v3) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = v0; e.val[1] = v1; e.val[2] = v2; e.val[3] = v3;
    }
    if (p) p(location, v0, v1, v2, v3);
}

typedef void (WINAPI *PFN_U1I)(GLint, GLint);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform1i(GLint location, GLint v0) {
    static PFN_U1I p = nullptr;
    if (!p) p = (PFN_U1I)glproxy::resolve("glUniform1i");
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLint, 1> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == v0) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = v0;
    }
    if (p) p(location, v0);
}
// glUniform2i dedup.
typedef void (WINAPI *PFN_U2I)(GLint, GLint, GLint);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform2i(GLint location, GLint v0, GLint v1) {
    static PFN_U2I p = nullptr;
    if (!p) p = (PFN_U2I)glproxy::resolve("glUniform2i");
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLint, 2> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == v0 && e.val[1] == v1) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = v0; e.val[1] = v1;
    }
    if (p) p(location, v0, v1);
}
GLP_EXT_FORWARD_VOID(Uniform3i, (GLint location, GLint v0, GLint v1, GLint v2), (location, v0, v1, v2))
GLP_EXT_FORWARD_VOID(Uniform4i, (GLint location, GLint v0, GLint v1, GLint v2, GLint v3), (location, v0, v1, v2, v3))
// glUniform1fv (count==1) dedup.
typedef void (WINAPI *PFN_U1FV)(GLint, GLsizei, const GLfloat*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform1fv(GLint location, GLsizei count, const GLfloat* value) {
    static PFN_U1FV p = nullptr;
    if (!p) p = (PFN_U1FV)glproxy::resolve("glUniform1fv");
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 1> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == value[0]) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = value[0];
    }
    if (p) p(location, count, value);
}
// glUniform2fv (count==1) dedup.
typedef void (WINAPI *PFN_U2FV)(GLint, GLsizei, const GLfloat*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform2fv(GLint location, GLsizei count, const GLfloat* value) {
    static PFN_U2FV p = nullptr;
    if (!p) p = (PFN_U2FV)glproxy::resolve("glUniform2fv");
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 2> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == value[0] && e.val[1] == value[1]) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = value[0]; e.val[1] = value[1];
    }
    if (p) p(location, count, value);
}
// glUniform3fv (count==1) dedup.
typedef void (WINAPI *PFN_U3FV)(GLint, GLsizei, const GLfloat*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform3fv(GLint location, GLsizei count, const GLfloat* value) {
    static PFN_U3FV p = nullptr;
    if (!p) p = (PFN_U3FV)glproxy::resolve("glUniform3fv");
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 3> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == value[0] && e.val[1] == value[1] && e.val[2] == value[2]) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = value[0]; e.val[1] = value[1]; e.val[2] = value[2];
    }
    if (p) p(location, count, value);
}

// glUniform4fv (count==1) dedup — used for color arrays, lighting params.
typedef void (WINAPI *PFN_U4FV)(GLint, GLsizei, const GLfloat*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform4fv(GLint location, GLsizei count, const GLfloat* value) {
    static PFN_U4FV p = nullptr;
    if (!p) p = (PFN_U4FV)glproxy::resolve("glUniform4fv");
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 4> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == value[0] && e.val[1] == value[1] &&
            e.val[2] == value[2] && e.val[3] == value[3]) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = value[0]; e.val[1] = value[1];
        e.val[2] = value[2]; e.val[3] = value[3];
    }
    if (p) p(location, count, value);
}
// glUniform1iv (count==1) dedup.
typedef void (WINAPI *PFN_U1IV)(GLint, GLsizei, const GLint*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform1iv(GLint location, GLsizei count, const GLint* value) {
    static PFN_U1IV p = nullptr;
    if (!p) p = (PFN_U1IV)glproxy::resolve("glUniform1iv");
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLint, 1> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == value[0]) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = value[0];
    }
    if (p) p(location, count, value);
}
// glUniform2iv (count==1) dedup.
typedef void (WINAPI *PFN_U2IV)(GLint, GLsizei, const GLint*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform2iv(GLint location, GLsizei count, const GLint* value) {
    static PFN_U2IV p = nullptr;
    if (!p) p = (PFN_U2IV)glproxy::resolve("glUniform2iv");
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLint, 2> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == value[0] && e.val[1] == value[1]) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = value[0]; e.val[1] = value[1];
    }
    if (p) p(location, count, value);
}
GLP_EXT_FORWARD_VOID(Uniform3iv, (GLint location, GLsizei count, const GLint* value), (location, count, value))
// glUniform4iv (count==1) dedup.
typedef void (WINAPI *PFN_U4IV)(GLint, GLsizei, const GLint*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform4iv(GLint location, GLsizei count, const GLint* value) {
    static PFN_U4IV p = nullptr;
    if (!p) p = (PFN_U4IV)glproxy::resolve("glUniform4iv");
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLint, 4> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == value[0] && e.val[1] == value[1] &&
            e.val[2] == value[2] && e.val[3] == value[3]) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = value[0]; e.val[1] = value[1];
        e.val[2] = value[2]; e.val[3] = value[3];
    }
    if (p) p(location, count, value);
}
GLP_EXT_FORWARD_VOID(UniformMatrix2fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (location, count, transpose, value))
// glUniformMatrix3fv (count==1) dedup — used by some custom shaders.
typedef void (WINAPI *PFN_UM3FV)(GLint, GLsizei, GLboolean, const GLfloat*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    static PFN_UM3FV p = nullptr;
    if (!p) p = (PFN_UM3FV)glproxy::resolve("glUniformMatrix3fv");
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 9> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            std::memcmp(e.val, value, sizeof(float) * 9) == 0) return;
        e.prog = g_currentProgram; e.loc = location;
        std::memcpy(e.val, value, sizeof(float) * 9);
    }
    if (p) p(location, count, transpose, value);
}

// glUniformMatrix4fv — dedup against last value per (program, location).
// Cocos2d-x uploads MVP matrix on every sprite draw even when identical.
// Uses lock-free direct-mapped fixed-size cache (no heap allocations on hot path).
typedef void (WINAPI *PFN_UM4FV)(GLint, GLsizei, GLboolean, const GLfloat*);
extern thread_local GLuint g_currentProgram;
extern "C" __declspec(dllexport) void WINAPI gl_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    static PFN_UM4FV p = nullptr;
    if (!p) p = (PFN_UM4FV)glproxy::resolve("glUniformMatrix4fv");
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        struct E { GLuint prog; GLint loc; float val[16]; };
        constexpr int N = 32;  // direct-mapped hash, power of 2
        thread_local E cache[N] = {};
        size_t h = ((size_t)g_currentProgram * 2654435761u + (size_t)location) & (N - 1);
        E& e = cache[h];
        if (e.prog == g_currentProgram && e.loc == location &&
            std::memcmp(e.val, value, sizeof(float) * 16) == 0) {
            return; // identical upload, skip driver call
        }
        e.prog = g_currentProgram;
        e.loc = location;
        std::memcpy(e.val, value, sizeof(float) * 16);
    }
    if (p) p(location, count, transpose, value);
}

// glVertexAttribPointer dedup — cocos2d sets identical layout (position/color/UV)
// on every sprite. Per-index cache of (size, type, normalized, stride, pointer)
// + currently-bound ARRAY_BUFFER. If all match — skip the call. ANGLE then
// avoids re-validating the vertex layout on each sprite (~1 µs saved per call).
//
// IMPORTANT: cache must invalidate when the bound ARRAY_BUFFER changes, because
// `pointer` is interpreted relative to the bound buffer. We track the buffer
// binding inside `gl_glBindBuffer` (target == GL_ARRAY_BUFFER) and clear the
// VAP cache there. (See above: `g_vapCacheArrayBuffer`.)
struct VAPCacheEntry {
    GLint    size;
    GLenum   type;
    GLboolean normalized;
    GLsizei  stride;
    const void* pointer;
    bool     valid;
};
static thread_local VAPCacheEntry g_vapCache[16] = {};
static thread_local GLuint g_vapCacheArrayBuffer = 0xFFFFFFFFu;
extern "C" void gdangle_invalidateVAPCache() {
    for (int i = 0; i < 16; i++) g_vapCache[i].valid = false;
}

typedef void (WINAPI *PFN_VAP)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
extern "C" __declspec(dllexport) void WINAPI gl_glVertexAttribPointer(
        GLuint index, GLint size, GLenum type, GLboolean normalized,
        GLsizei stride, const void* pointer) {
    static PFN_VAP p = nullptr;
    if (!p) p = (PFN_VAP)glproxy::resolve("glVertexAttribPointer");
    if (index < 16) {
        VAPCacheEntry& e = g_vapCache[index];
        if (e.valid &&
            e.size == size && e.type == type &&
            e.normalized == normalized && e.stride == stride &&
            e.pointer == pointer) return;
        e.size = size; e.type = type; e.normalized = normalized;
        e.stride = stride; e.pointer = pointer; e.valid = true;
    }
    if (p) p(index, size, type, normalized, stride, pointer);
}

// VertexAttribArray enable/disable dedup. cocos2d toggles attrib 0/1/2/3
// (position, color, uv, ...) on every sprite. Bitmask tracks current state.
typedef void (WINAPI *PFN_EVAA)(GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glEnableVertexAttribArray(GLuint index) {
    static PFN_EVAA p = nullptr;
    if (!p) p = (PFN_EVAA)glproxy::resolve("glEnableVertexAttribArray");
    if (index < 32) {
        unsigned int mask = 1u << index;
        if (g_vaaEnabledMask & mask) return;
        g_vaaEnabledMask |= mask;
    }
    if (p) p(index);
}
extern "C" __declspec(dllexport) void WINAPI gl_glDisableVertexAttribArray(GLuint index) {
    static PFN_EVAA p = nullptr;
    if (!p) p = (PFN_EVAA)glproxy::resolve("glDisableVertexAttribArray");
    if (index < 32) {
        unsigned int mask = 1u << index;
        if (!(g_vaaEnabledMask & mask)) return;
        g_vaaEnabledMask &= ~mask;
    }
    if (p) p(index);
}
GLP_EXT_FORWARD_VOID(VertexAttrib1f, (GLuint index, GLfloat x), (index, x))
GLP_EXT_FORWARD_VOID(VertexAttrib2f, (GLuint index, GLfloat x, GLfloat y), (index, x, y))
GLP_EXT_FORWARD_VOID(VertexAttrib3f, (GLuint index, GLfloat x, GLfloat y, GLfloat z), (index, x, y, z))
GLP_EXT_FORWARD_VOID(VertexAttrib4f, (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w), (index, x, y, z, w))
GLP_EXT_FORWARD_VOID(VertexAttrib1fv, (GLuint index, const GLfloat* v), (index, v))
GLP_EXT_FORWARD_VOID(VertexAttrib2fv, (GLuint index, const GLfloat* v), (index, v))
GLP_EXT_FORWARD_VOID(VertexAttrib3fv, (GLuint index, const GLfloat* v), (index, v))
GLP_EXT_FORWARD_VOID(VertexAttrib4fv, (GLuint index, const GLfloat* v), (index, v))

// Framebuffers
GLP_EXT_FORWARD_VOID(GenFramebuffers, (GLsizei n, GLuint* framebuffers), (n, framebuffers))

// glBindFramebuffer per-target dedup + diagnostic logging for first 30 calls.
// Also: halfres_render redirect — when halfres is active and caller asks for
// the default backbuffer (FB 0), redirect to our offscreen FBO.
extern "C" GLuint gdangle_halfresRedirectFB(GLuint requested);  // boost_halfres_render.cpp
typedef void (WINAPI *PFN_BFB)(GLenum, GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    static PFN_BFB p = nullptr;
    if (!p) p = (PFN_BFB)glproxy::resolve("glBindFramebuffer");

    // Halfres: redirect FB 0 -> offscreen FBO. No-op when halfres disabled.
    GLuint actualFB = gdangle_halfresRedirectFB(framebuffer);

    thread_local GLuint t_read = 0xFFFFFFFFu, t_draw = 0xFFFFFFFFu;
    bool needCall = true;
    switch (target) {
        case 0x8D40: // GL_FRAMEBUFFER — sets both R and D
            if (t_read == actualFB && t_draw == actualFB) needCall = false;
            t_read = t_draw = actualFB;
            break;
        case 0x8CA8: // GL_READ_FRAMEBUFFER
            if (t_read == actualFB) needCall = false;
            t_read = actualFB;
            break;
        case 0x8CA9: // GL_DRAW_FRAMEBUFFER
            if (t_draw == actualFB) needCall = false;
            t_draw = actualFB;
            break;
        default: break;
    }
    static int n = 0;
    if (n < 30) angle::log("glBindFramebuffer #%d: target=0x%X fb=%u%s%s", n, target, framebuffer,
                            (actualFB != framebuffer) ? " [halfres-redirect]" : "",
                            needCall ? "" : " [dedup]");
    n++;
    if (needCall && p) p(target, actualFB);
}
GLP_EXT_FORWARD_VOID(FramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level), (target, attachment, textarget, texture, level))
GLP_EXT_FORWARD_VOID(FramebufferRenderbuffer, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer), (target, attachment, renderbuffertarget, renderbuffer))
GLP_EXT_FORWARD(GLenum, CheckFramebufferStatus, (GLenum target), (target))
GLP_EXT_FORWARD_VOID(DeleteFramebuffers, (GLsizei n, const GLuint* framebuffers), (n, framebuffers))
GLP_EXT_FORWARD(GLboolean, IsFramebuffer, (GLuint framebuffer), (framebuffer))
GLP_EXT_FORWARD_VOID(BlitFramebuffer, (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter), (srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter))
GLP_EXT_FORWARD_VOID(GetFramebufferAttachmentParameteriv, (GLenum target, GLenum attachment, GLenum pname, GLint* params), (target, attachment, pname, params))

// Renderbuffers
GLP_EXT_FORWARD_VOID(GenRenderbuffers, (GLsizei n, GLuint* renderbuffers), (n, renderbuffers))
// glBindRenderbuffer dedup — only one target (GL_RENDERBUFFER), thread-local.
typedef void (WINAPI *PFN_BRB)(GLenum, GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    static PFN_BRB p = nullptr;
    if (!p) p = (PFN_BRB)glproxy::resolve("glBindRenderbuffer");
    if (target == 0x8D41) {  // GL_RENDERBUFFER
        thread_local GLuint cur = 0xFFFFFFFFu;
        if (cur == renderbuffer) return;
        cur = renderbuffer;
    }
    if (p) p(target, renderbuffer);
}
GLP_EXT_FORWARD_VOID(RenderbufferStorage, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height), (target, internalformat, width, height))
GLP_EXT_FORWARD_VOID(RenderbufferStorageMultisample, (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height), (target, samples, internalformat, width, height))
GLP_EXT_FORWARD_VOID(DeleteRenderbuffers, (GLsizei n, const GLuint* renderbuffers), (n, renderbuffers))
GLP_EXT_FORWARD(GLboolean, IsRenderbuffer, (GLuint renderbuffer), (renderbuffer))

// Misc
GLP_EXT_FORWARD_VOID(StencilFuncSeparate, (GLenum face, GLenum func, GLint ref, GLuint mask), (face, func, ref, mask))
GLP_EXT_FORWARD_VOID(StencilMaskSeparate, (GLenum face, GLuint mask), (face, mask))
GLP_EXT_FORWARD_VOID(StencilOpSeparate, (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass), (face, sfail, dpfail, dppass))
// gl_glSampleCoverage moved to gl_proxy.cpp with dedup.
