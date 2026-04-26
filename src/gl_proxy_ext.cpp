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

// glDrawBuffer is desktop-GL only; ANGLE/GLES has only the plural glDrawBuffers.
// Compatibility shim for mods written against desktop GL (peony.silicate etc.):
// translate the singular form into the plural form.
typedef void (WINAPI *PFN_DB_PLURAL)(GLsizei, const GLenum*);
extern "C" __declspec(dllexport) void WINAPI gl_glDrawBuffer(GLenum mode) {
    static PFN_DB_PLURAL p = nullptr;
    if (!p) p = (PFN_DB_PLURAL)glproxy::resolve("glDrawBuffers");
    if (p) p(1, &mode);
}

// glBindSampler — GLES 3.0 sampler-object binding. Direct forward.
GLP_EXT_FORWARD_VOID(BindSampler, (GLuint unit, GLuint sampler), (unit, sampler))

// glBlendEquationSeparate — GLES 2.0+, separate RGB / A equations. Direct forward.
GLP_EXT_FORWARD_VOID(BlendEquationSeparate, (GLenum modeRGB, GLenum modeAlpha), (modeRGB, modeAlpha))

// Vertex-attrib query getters — GLES 2.0+, direct forward.
GLP_EXT_FORWARD_VOID(GetVertexAttribiv, (GLuint index, GLenum pname, GLint* params), (index, pname, params))
GLP_EXT_FORWARD_VOID(GetVertexAttribPointerv, (GLuint index, GLenum pname, void** pointer), (index, pname, pointer))

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
//
// File-scope so glBufferData / glBufferSubData can also query "is anything
// bound to this target?" — ANGLE crashes if the answer is no, desktop-GL
// silently no-ops (some Geode mods rely on the silent behaviour).
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
thread_local GLuint g_bufferBindings[8] = {
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu
};
// Returns true if the caller asked us to operate on a buffer target with no
// real buffer bound (binding is 0 or sentinel "never set"). ANGLE crashes,
// desktop-GL no-ops; we mimic desktop.
static inline bool gd_noBufferBound(GLenum target) {
    int slot = s_bufTargetSlot(target);
    if (slot < 0) return false;  // unknown target — let ANGLE handle
    GLuint b = g_bufferBindings[slot];
    return b == 0 || b == 0xFFFFFFFFu;
}
extern "C" void gdangle_invalidateVAPCache();  // forward decl
typedef void (WINAPI *PFN_BB)(GLenum, GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glBindBuffer(GLenum target, GLuint buffer) {
    static PFN_BB p = nullptr;
    if (!p) p = (PFN_BB)glproxy::resolve("glBindBuffer");
    int slot = s_bufTargetSlot(target);
    if (slot >= 0) {
        if (g_bufferBindings[slot] == buffer) return;
        g_bufferBindings[slot] = buffer;
        // ARRAY_BUFFER change → VAP cache must be invalidated (pointer offsets
        // are interpreted relative to the bound array buffer).
        if (slot == 0) gdangle_invalidateVAPCache();
    }
    if (p) p(target, buffer);
}
// glBufferData / glBufferSubData — desktop-GL parity guard: no-op when no
// buffer is bound to the target (ANGLE crashes deep in libGLESv2 otherwise).
typedef void (WINAPI *PFN_BD)(GLenum, GLsizeiptr, const void*, GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    static PFN_BD p = nullptr;
    if (!p) p = (PFN_BD)glproxy::resolve("glBufferData");
    if (gd_noBufferBound(target)) return;  // desktop-GL parity: silent no-op
    if (p) p(target, size, data, usage);
}
typedef void (WINAPI *PFN_BSD)(GLenum, GLintptr, GLsizeiptr, const void*);
extern "C" __declspec(dllexport) void WINAPI gl_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    static PFN_BSD p = nullptr;
    if (!p) p = (PFN_BSD)glproxy::resolve("glBufferSubData");
    if (gd_noBufferBound(target)) return;  // desktop-GL parity: silent no-op
    if (p) p(target, offset, size, data);
}
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

// Link-status tracking. ANGLE crashes (__fastfail / illegal instruction
// inside libGLESv2's stream-translator) when glDrawArrays / glDrawElements
// is called with a non-linked program currently bound — this is desktop-GL
// "tolerated, draws nothing" vs ANGLE "fastfail and take down the game".
//
// peony.silicate ships shaders with desktop-only #extension directives
// (GL_ARB_explicit_attrib_location, GL_ARB_explicit_uniform_location);
// even after our shader translator strips those, the underlying
// `layout(location=N) uniform` syntax still fails in ESSL3, so the
// program never links. Silicate then proceeds to use the broken program
// for drawing — desktop-GL would just produce nothing, ANGLE crashes.
//
// Track each program's last-known link status. gl_glDrawArrays /
// glDrawElements consult this map (via gdangle_currentProgramOK) and
// silently skip the draw if the bound program failed to link.
static std::unordered_map<GLuint, bool> g_programLinked;

typedef GLuint (WINAPI *PFN_CS)(GLenum);
extern "C" __declspec(dllexport) GLuint WINAPI gl_glCreateShader(GLenum type) {
    static PFN_CS p = nullptr;
    if (!p) p = (PFN_CS)glproxy::resolve("glCreateShader");
    GLuint id = p ? p(type) : 0;
    if (id) g_shaderType[id] = type;
    return id;
}
GLP_EXT_FORWARD_VOID(DeleteShader, (GLuint shader), (shader))

// Patch shader source for ANGLE GLES backend compatibility.
//
// Two distinct cases handled:
//
// (1) Source has NO #version directive (cocos2d-x 2.2 GD shaders):
//     Prepend "#version 100\nprecision mediump ...\n" so ANGLE's GLES
//     translator accepts the legacy desktop GLSL 1.10 syntax.
//
// (2) Source HAS a #version directive (Eclipse Menu / ImGui shaders, mods):
//     - GLSL #version values map to ES like so:
//         110 / 120          (attribute/varying syntax) -> "#version 100"
//         130 / 140 / 150 /
//         330 / 400 / 410+   (in/out syntax)            -> "#version 300 es"
//         100 (already ES)                              -> keep as-is
//         300 es / 310 es / 320 es                      -> keep as-is
//     - The #version directive MUST be the first non-comment/whitespace
//       token in the source. So we replace the existing line in-place
//       and inject the precision qualifier on the line *after* it, never
//       before. (Previous version prepended precision unconditionally,
//       which broke shaders that already had #version because ANGLE
//       complained "'version' directive must occur before anything else".)
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

    // Locate the #version directive (skipping leading comments/whitespace)
    size_t versionPos = src.find("#version");
    std::string patched;

    if (versionPos == std::string::npos) {
        // Case 1: no #version — legacy cocos2d-x style. Prepend ES 1.00 +
        // precision, then the original source as-is.
        std::string prefix = "#version 100\n";
        if (isFragment) prefix += "precision mediump float;\nprecision mediump int;\n";
        else            prefix += "precision mediump int;\n";
        patched = prefix + src;
    } else {
        // Case 2: source has #version. Find the end of that line.
        size_t lineEnd = src.find('\n', versionPos);
        if (lineEnd == std::string::npos) lineEnd = src.size();
        std::string verLine = src.substr(versionPos, lineEnd - versionPos);

        // Parse the version number. Format: "#version <NNN>[ es]?"
        int verNum = 0;
        bool isEs = verLine.find(" es") != std::string::npos;
        size_t numPos = verLine.find_first_of("0123456789");
        if (numPos != std::string::npos) {
            for (size_t i = numPos; i < verLine.size() && verLine[i] >= '0' && verLine[i] <= '9'; i++) {
                verNum = verNum * 10 + (verLine[i] - '0');
            }
        }

        // Decide on the translated #version line.
        std::string newVerLine;
        if (isEs) {
            // Already ES — leave alone (100, 300 es, 310 es, 320 es)
            newVerLine = verLine;
        } else if (verNum <= 120) {
            // Desktop 1.1x — uses attribute/varying, maps to ES 1.00
            newVerLine = "#version 100";
        } else {
            // Desktop 1.30+ / 3.30+ — uses in/out, maps to ES 3.00
            newVerLine = "#version 300 es";
        }

        // Anything before #version (comments, whitespace) is preserved.
        std::string before = src.substr(0, versionPos);

        // ESSL3 rule: ALL #extension directives must precede any
        // non-preprocessor tokens (including `precision` declarations).
        // So walk forward past every consecutive #extension / blank /
        // comment line, then inject precision *after* that block.
        // Without this, shaders that use #extension (e.g. peony.silicate's
        // render-pass shaders) fail to compile with:
        //   "extension directive must occur before any non-preprocessor
        //    tokens in ESSL3"
        //
        // While walking, we also COMMENT OUT desktop-only #extension
        // directives that map to built-in features in GLES 3.00 (or that
        // simply don't exist there). ANGLE rejects them otherwise.
        bool toEs3 = (newVerLine == "#version 300 es");
        std::string head;  // built up incrementally; replaces src[lineEnd:injectPos]
        size_t injectPos = lineEnd;  // start at the '\n' after #version
        while (injectPos < src.size()) {
            size_t lineBeg = injectPos;
            // Skip the leading '\n' of the line we just consumed.
            if (src[lineBeg] == '\n') lineBeg++;
            // Find the next non-whitespace character on this line.
            size_t textStart = lineBeg;
            while (textStart < src.size() &&
                   (src[textStart] == ' ' || src[textStart] == '\t')) textStart++;
            // Detect a preprocessor line we should preserve before precision.
            bool isExt =
                src.compare(textStart, 10, "#extension") == 0;
            // Detect a fully blank line or a // line comment — both safe to skip.
            bool isBlank = (textStart >= src.size() || src[textStart] == '\n');
            bool isLineComment =
                textStart + 2 <= src.size() && src[textStart] == '/' && src[textStart+1] == '/';
            if (!(isExt || isBlank || isLineComment)) break;
            // Advance to (and consume) the next '\n'.
            size_t nextNl = src.find('\n', textStart);
            size_t lineEndPos = (nextNl == std::string::npos) ? src.size() : nextNl;

            // Decide whether to keep, strip, or pass through this line.
            std::string line = src.substr(lineBeg, lineEndPos - lineBeg);
            if (isExt && toEs3) {
                // Desktop-only ARB extensions that have no ES 3.00 equivalent
                // OR whose feature is built-in to ES 3.00 — strip them out
                // (replace with comment to preserve line count for error
                // reporting). Keep ES-compatible extensions (e.g.
                // GL_OES_*, GL_EXT_shader_texture_lod) which ANGLE handles.
                static const char* kStripExt[] = {
                    "GL_ARB_explicit_attrib_location",   // built-in to ES 3.00
                    "GL_ARB_explicit_uniform_location",  // not in ES 3.00, no replacement
                    "GL_ARB_separate_shader_objects",
                    "GL_ARB_shading_language_420pack",
                    "GL_ARB_enhanced_layouts",
                    "GL_ARB_uniform_buffer_object",      // built-in to ES 3.00
                    "GL_ARB_texture_rectangle",
                    "GL_ARB_sample_shading",
                    "GL_ARB_gpu_shader5",
                };
                bool stripped = false;
                for (auto* name : kStripExt) {
                    if (line.find(name) != std::string::npos) {
                        line = "// stripped (desktop-only): " + line;
                        stripped = true;
                        break;
                    }
                }
                (void)stripped;
            }
            head.push_back('\n');
            head.append(line);

            if (nextNl == std::string::npos) { injectPos = src.size(); break; }
            injectPos = nextNl;  // points at '\n'; next loop iter consumes it
        }
        std::string tail = src.substr(injectPos);

        // Inject precision AFTER the #version + #extension block. Required
        // for ES 3.00 fragment shaders (no default precision for
        // float/sampler) and harmless redundancy on vertex shaders.
        std::string precisionInject;
        if (isFragment) precisionInject = "\nprecision mediump float;\nprecision mediump int;";
        else            precisionInject = "\nprecision mediump int;";

        patched = before + newVerLine + head + precisionInject + tail;
    }

    const GLchar* ptrs[1] = { patched.c_str() };
    GLint lens[1] = { (GLint)patched.size() };
    p(shader, 1, ptrs, lens);

    static int n = 0;
    if (n < 8) angle::log("ShaderSource patch #%d shader=%u type=%s (orig %zu B -> patched %zu B, hasVersion=%d)",
                          n, shader, isFragment ? "FRAG" : "VERT",
                          src.size(), patched.size(), versionPos != std::string::npos);
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

// glDeleteProgram — keep our `g_currentProgram` cache valid: if we just
// deleted the program that was currently bound, ANGLE will set
// GL_CURRENT_PROGRAM to 0 internally, so reflect that in our cache too.
// Without this, gl_glUniform* calls after DeleteProgram of the current
// program would crash deep in ANGLE (null deref on the freed Program*).
typedef void (WINAPI *PFN_DP)(GLuint);
extern thread_local GLuint g_currentProgram;
extern "C" __declspec(dllexport) void WINAPI gl_glDeleteProgram(GLuint program) {
    static PFN_DP p = nullptr;
    if (!p) p = (PFN_DP)glproxy::resolve("glDeleteProgram");
    if (p) p(program);
    if (program != 0 && program == g_currentProgram) {
        g_currentProgram = 0;
    }
    // Drop link-status entry; the GLuint may be recycled for a new program.
    g_programLinked.erase(program);
}
GLP_EXT_FORWARD_VOID(AttachShader, (GLuint program, GLuint shader), (program, shader))
GLP_EXT_FORWARD_VOID(DetachShader, (GLuint program, GLuint shader), (program, shader))
typedef void (WINAPI *PFN_LP)(GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glLinkProgram(GLuint program) {
    static PFN_LP p = nullptr;
    if (!p) p = (PFN_LP)glproxy::resolve("glLinkProgram");
    if (p) p(program);
    // Query link status; cache it so subsequent draw calls can skip work
    // when a mod hooked us up with a non-linked program (silicate's path).
    typedef void (WINAPI *PFN_GPV)(GLuint, GLenum, GLint*);
    static PFN_GPV pgpiv = nullptr;
    if (!pgpiv) pgpiv = (PFN_GPV)glproxy::resolve("glGetProgramiv");
    GLint ls = 1;  // assume OK if we can't query
    if (pgpiv) pgpiv(program, 0x8B82 /*GL_LINK_STATUS*/, &ls);
    g_programLinked[program] = (ls != 0);
    static int n = 0;
    if (n < 30) {
        angle::log("glLinkProgram #%d: prog=%u link_status=%d", n, program, ls);
        if (ls == 0 && pgpiv) {
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

// Called by gl_glDrawArrays / glDrawElements / glDrawElementsBaseVertex
// (in gl_proxy.cpp) to decide whether forwarding the draw to ANGLE is safe.
// Returns false when the currently bound program has GL_LINK_STATUS=0
// (ANGLE fastfails on drawing with such programs).
//
// Critical: peony.silicate calls glShaderSource / glLinkProgram / glUseProgram
// *directly* via GetProcAddress(libGLESv2.dll, ...), bypassing our proxy
// hooks entirely. So our `g_currentProgram` cache and `g_programLinked` map
// only contain GD/cocos2d programs. To catch silicate's broken programs
// at draw time we must query ANGLE itself.
//
// Hot-path optimisation: cache the last-queried (program, ok) pair
// thread-locally. Re-query only when the current program ID changes —
// glGetIntegerv is cheap (~100 ns, no GPU roundtrip), but doing one per
// draw call still costs ~1 ms/frame in heavy scenes. Re-querying only
// on transitions drops that to near zero.
typedef void (WINAPI *PFN_GIV3)(GLenum, GLint*);
typedef void (WINAPI *PFN_GPV2)(GLuint, GLenum, GLint*);
extern "C" bool gdangle_currentProgramOK() {
    static PFN_GIV3 pGetIv = nullptr;
    static PFN_GPV2 pGetPv = nullptr;
    if (!pGetIv) pGetIv = (PFN_GIV3)glproxy::resolve("glGetIntegerv");
    if (!pGetPv) pGetPv = (PFN_GPV2)glproxy::resolve("glGetProgramiv");
    if (!pGetIv || !pGetPv) return true;  // can't verify → assume OK

    GLint cur = 0;
    pGetIv(0x8B8D /*GL_CURRENT_PROGRAM*/, &cur);
    if (cur <= 0) return false;  // no program bound → ANGLE fastfails

    // Per-thread cache of last-seen (current program, OK?) pair.
    thread_local GLuint t_lastProg = 0xFFFFFFFFu;
    thread_local bool   t_lastOK   = true;
    if ((GLuint)cur == t_lastProg) return t_lastOK;

    // Program changed (or first time) — consult tracking map first, then
    // fall back to a one-time GL query. Cache the result for next call.
    bool ok;
    auto it = g_programLinked.find((GLuint)cur);
    if (it != g_programLinked.end()) {
        ok = it->second;
    } else {
        GLint ls = 1;
        pGetPv((GLuint)cur, 0x8B82 /*GL_LINK_STATUS*/, &ls);
        ok = (ls != 0);
        g_programLinked[(GLuint)cur] = ok;
    }
    t_lastProg = (GLuint)cur;
    t_lastOK   = ok;
    return ok;
}

typedef void (WINAPI *PFN_UP)(GLuint);
typedef void (WINAPI *PFN_GIV)(GLenum, GLint*);
// thread_local current program — used by uniform dedup and dgd_noProgramBound.
thread_local GLuint g_currentProgram = 0xFFFFFFFFu;
extern "C" __declspec(dllexport) void WINAPI gl_glUseProgram(GLuint program) {
    static PFN_UP  p     = nullptr;
    static PFN_GIV pGetI = nullptr;
    if (!p)     p     = (PFN_UP) glproxy::resolve("glUseProgram");
    if (!pGetI) pGetI = (PFN_GIV)glproxy::resolve("glGetIntegerv");
    if (program == g_currentProgram) return;
    if (p) p(program);
    // Trust-but-verify ONCE per glUseProgram (rare event vs uniform calls):
    // ANGLE may have rejected the bind (invalid program ID, link failure, etc).
    // If so, GL_CURRENT_PROGRAM stays at its previous value (often 0). Cache
    // the truth so downstream gl_glUniform* dedup + gd_noProgramBound stay
    // consistent with what ANGLE actually thinks is bound.
    if (pGetI) {
        GLint actual = 0;
        pGetI(0x8B8D /*GL_CURRENT_PROGRAM*/, &actual);
        g_currentProgram = (GLuint)actual;
    } else {
        g_currentProgram = program;  // fallback: trust the request
    }
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

// Compatibility shim: ANGLE crashes in glUniform* if no program is currently
// bound (null deref deep inside ANGLE's program-state lookup). Desktop OpenGL
// silently no-ops this case. Some Geode mods (peony.silicate) rely on the
// desktop-GL behaviour and call glUniform* without ensuring a program is
// bound, leading to crashes only when ReviANGLE is active. Match desktop
// behaviour to keep these mods alive.
//
// Cache-only check (hot path — ~10000 calls/frame from cocos2d). Cache
// validity is maintained by:
//   - gl_glUseProgram:   queries GL_CURRENT_PROGRAM after each bind to
//                        catch ANGLE rejecting invalid program IDs.
//   - gl_glDeleteProgram: clears cache to 0 if deleting the bound program.
// This keeps gd_noProgramBound() at a single integer compare (~1 ns) instead
// of a per-uniform glGetIntegerv (which destroyed framerate on D3D11).
static inline bool gd_noProgramBound() {
    return g_currentProgram == 0 || g_currentProgram == 0xFFFFFFFFu;
}

typedef void (WINAPI *PFN_U1F)(GLint, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform1f(GLint location, GLfloat v0) {
    static PFN_U1F p = nullptr;
    if (!p) p = (PFN_U1F)glproxy::resolve("glUniform1f");
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;
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
    if (gd_noProgramBound()) return;  // desktop-GL parity: silent no-op
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

// Reset every thread_local state cache we maintain. Called from
// wgl_wglMakeCurrent when the EGL context changes (e.g. after
// CCEGLView::toggleFullScreen). Cocos2d-x destroys and recreates the GL
// context across fullscreen toggles, but our thread_local dedup caches
// would otherwise survive the recreation, causing dedup'd binds to skip
// real work on the new context and producing null-deref crashes on
// follow-up state-dependent calls.
extern thread_local GLuint g_currentRBO;  // forward decl (defined later)
extern "C" void gdangle_invalidateAllStateCaches() {
    g_currentProgram = 0xFFFFFFFFu;
    g_currentVAO     = 0xFFFFFFFFu;
    g_vaaEnabledMask = 0;
    g_currentRBO     = 0xFFFFFFFFu;
    for (int i = 0; i < 8; i++) g_bufferBindings[i] = 0xFFFFFFFFu;
    g_vapCacheArrayBuffer = 0xFFFFFFFFu;
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

// glBindFramebuffer — no dedup. FB binds are not on the hot path (cocos2d
// does them only for render-to-texture passes, ~tens per frame max).
//
// Previous version had per-target dedup, but the thread_local cache survived
// GL context recreation (e.g. CCEGLView::toggleFullScreen rebuilds the
// context). After recreation, the cache held stale FB IDs; eclipse-menu /
// silicate rebinding "the same" FB got dedup'd, but ANGLE's new context had
// no FB bound, leading to null-deref crashes inside follow-up
// glRenderbufferStorage / glFramebufferTexture2D calls. Removing the dedup
// trades negligible perf for correctness.
//
// Halfres redirect is preserved: FB 0 → offscreen FBO when active.
extern "C" GLuint gdangle_halfresRedirectFB(GLuint requested);  // boost_halfres_render.cpp
typedef void (WINAPI *PFN_BFB)(GLenum, GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    static PFN_BFB p = nullptr;
    if (!p) p = (PFN_BFB)glproxy::resolve("glBindFramebuffer");
    GLuint actualFB = gdangle_halfresRedirectFB(framebuffer);
    static int n = 0;
    if (n < 30) angle::log("glBindFramebuffer #%d: target=0x%X fb=%u%s", n, target, framebuffer,
                            (actualFB != framebuffer) ? " [halfres-redirect]" : "");
    n++;
    if (p) p(target, actualFB);
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

// glBindRenderbuffer — no dedup, plus track the binding ourselves so that
// downstream glRenderbufferStorage* can verify a renderbuffer is actually
// bound before forwarding (ANGLE crashes if not, desktop-GL silently fails).
thread_local GLuint g_currentRBO = 0xFFFFFFFFu;
typedef void (WINAPI *PFN_BRB)(GLenum, GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    static PFN_BRB p = nullptr;
    if (!p) p = (PFN_BRB)glproxy::resolve("glBindRenderbuffer");
    if (target == 0x8D41 /*GL_RENDERBUFFER*/) g_currentRBO = renderbuffer;
    if (p) p(target, renderbuffer);
}
// glRenderbufferStorage / Multisample — desktop-GL parity guard. ANGLE
// crashes (null deref deep in libGLESv2) if no RBO is bound to GL_RENDERBUFFER
// at call time. Cocos2d's CCEGLView::updateWindow (called during fullscreen
// toggle) sometimes invokes RenderbufferStorage before the calling code has
// re-bound a fresh RBO in the recreated context. Cache check is cheap and
// avoids a one-shot ANGLE crash that takes the whole game down.
//
// If our cache is stale (caller bypassed our proxy for the bind, or the
// previous bind was invalidated by context recreation), trust-but-verify
// with glGetIntegerv(GL_RENDERBUFFER_BINDING). RBO storage calls are very
// rare (~tens per session), so the extra GL state query is free.
typedef void (WINAPI *PFN_GIV2)(GLenum, GLint*);
static bool gd_noRBOBound() {
    if (g_currentRBO != 0 && g_currentRBO != 0xFFFFFFFFu) return false;
    static PFN_GIV2 pGetIv = nullptr;
    if (!pGetIv) pGetIv = (PFN_GIV2)glproxy::resolve("glGetIntegerv");
    if (!pGetIv) return false;
    GLint cur = 0;
    pGetIv(0x8CA7 /*GL_RENDERBUFFER_BINDING*/, &cur);
    g_currentRBO = (GLuint)cur;
    return cur == 0;
}
typedef void (WINAPI *PFN_RBS)(GLenum, GLenum, GLsizei, GLsizei);
extern "C" __declspec(dllexport) void WINAPI gl_glRenderbufferStorage(
        GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    static PFN_RBS p = nullptr;
    if (!p) p = (PFN_RBS)glproxy::resolve("glRenderbufferStorage");
    if (gd_noRBOBound()) return;  // desktop-GL parity: silent no-op
    if (p) p(target, internalformat, width, height);
}
typedef void (WINAPI *PFN_RBSM)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
extern "C" __declspec(dllexport) void WINAPI gl_glRenderbufferStorageMultisample(
        GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    static PFN_RBSM p = nullptr;
    if (!p) p = (PFN_RBSM)glproxy::resolve("glRenderbufferStorageMultisample");
    if (gd_noRBOBound()) return;  // desktop-GL parity: silent no-op
    if (p) p(target, samples, internalformat, width, height);
}
GLP_EXT_FORWARD_VOID(DeleteRenderbuffers, (GLsizei n, const GLuint* renderbuffers), (n, renderbuffers))
GLP_EXT_FORWARD(GLboolean, IsRenderbuffer, (GLuint renderbuffer), (renderbuffer))

// Misc
GLP_EXT_FORWARD_VOID(StencilFuncSeparate, (GLenum face, GLenum func, GLint ref, GLuint mask), (face, func, ref, mask))
GLP_EXT_FORWARD_VOID(StencilMaskSeparate, (GLenum face, GLuint mask), (face, mask))
GLP_EXT_FORWARD_VOID(StencilOpSeparate, (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass), (face, sfail, dpfail, dppass))
// gl_glSampleCoverage moved to gl_proxy.cpp with dedup.
