// Boost: Half-Resolution Rendering
//
// Renders the entire game at half resolution (W/2 x H/2) into an offscreen
// FBO, then upscales to the real backbuffer (W x H) on Present via
// glBlitFramebuffer with GL_LINEAR filtering.
//
// Net effect: GPU does 4x less fragment shading work. On weak GPUs (GT 630M
// fillrate ~3.5 GP/s) this can mean 30-50% FPS gain on heavy levels.
//
// Tradeoffs:
//   - Visible quality drop. UI text may look blurrier.
//   - FBO allocation: 1080p backbuffer -> 540p RGBA8 + depth-stencil
//     = ~2.2 + 2.2 MB VRAM = ~4.5 MB. Negligible.
//
// Implementation:
//   1. After eglMakeCurrent succeeds (post-GL-init), allocate the offscreen
//      FBO at half the backbuffer's resolution.
//   2. Hook glBindFramebuffer(target, 0): redirect to bind our offscreen FBO
//      so all "render to default backbuffer" calls go to half-res.
//   3. Hook glViewport(x, y, w, h): if (w, h) match the real backbuffer
//      size, halve them. Other viewports (FBO render-to-texture for effects)
//      pass through unchanged.
//   4. Hook wglSwapBuffers preamble: bypass our hooks via raw EGL pointers,
//      blit the offscreen FBO to the real default FB, then proceed with
//      Present.
//
// State tracking:
//   - g_realW, g_realH: real backbuffer size (set by init)
//   - g_offFBO: the FBO ID we allocated
//   - g_active: true once init succeeded
//
// Opt-in via [BoostPipeline] halfres_render = true.
//
// NOTE: This is an ADVANCED module. If anything goes wrong (e.g. ANGLE
//       backend doesn't support glBlitFramebuffer in the right mode),
//       the screen will go black. Users should test on a known-good level
//       before relying on it.

#include <windows.h>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int  GLenum;
typedef unsigned int  GLuint;
typedef unsigned int  GLbitfield;
typedef int           GLint;
typedef int           GLsizei;
typedef unsigned char GLboolean;

namespace boost_halfres_render {

// =====================================================================
// State
// =====================================================================
static bool   s_active = false;
static GLint  s_realW  = 0;
static GLint  s_realH  = 0;
static GLuint s_offFBO     = 0;
static GLuint s_offColor   = 0;
static GLuint s_offDS      = 0;  // depth-stencil renderbuffer

// Raw function pointers - bypass our own dedup hooks during internal use.
typedef void     (WINAPI *PFN_GenFB)(GLsizei, GLuint*);
typedef void     (WINAPI *PFN_BindFB)(GLenum, GLuint);
typedef void     (WINAPI *PFN_GenTex)(GLsizei, GLuint*);
typedef void     (WINAPI *PFN_BindTex)(GLenum, GLuint);
typedef void     (WINAPI *PFN_TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
typedef void     (WINAPI *PFN_TexParam)(GLenum, GLenum, GLint);
typedef void     (WINAPI *PFN_GenRB)(GLsizei, GLuint*);
typedef void     (WINAPI *PFN_BindRB)(GLenum, GLuint);
typedef void     (WINAPI *PFN_RBStorage)(GLenum, GLenum, GLsizei, GLsizei);
typedef void     (WINAPI *PFN_FBTex2D)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef void     (WINAPI *PFN_FBRB)(GLenum, GLenum, GLenum, GLuint);
typedef GLenum   (WINAPI *PFN_FBStat)(GLenum);
typedef void     (WINAPI *PFN_Viewport)(GLint, GLint, GLsizei, GLsizei);
typedef void     (WINAPI *PFN_Blit)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
typedef void     (WINAPI *PFN_GetIv)(GLenum, GLint*);

static PFN_BindFB   raw_bindFB    = nullptr;
static PFN_Viewport raw_viewport  = nullptr;
static PFN_Blit     raw_blit      = nullptr;
static PFN_GetIv    raw_getIv     = nullptr;

// =====================================================================
// Init - called from wgl_proxy.cpp post-eglMakeCurrent
// =====================================================================
extern "C" void gdangle_halfresInit(int realW, int realH) {
    if (!Config::get().halfres_render) return;
    if (s_active) return;
    if (realW <= 0 || realH <= 0) return;

    auto rGenFB    = (PFN_GenFB)    glproxy::resolve("glGenFramebuffers");
    auto rBindFB   = (PFN_BindFB)   glproxy::resolve("glBindFramebuffer");
    auto rGenTex   = (PFN_GenTex)   glproxy::resolve("glGenTextures");
    auto rBindTex  = (PFN_BindTex)  glproxy::resolve("glBindTexture");
    auto rTexImg   = (PFN_TexImage2D)glproxy::resolve("glTexImage2D");
    auto rTexParam = (PFN_TexParam) glproxy::resolve("glTexParameteri");
    auto rGenRB    = (PFN_GenRB)    glproxy::resolve("glGenRenderbuffers");
    auto rBindRB   = (PFN_BindRB)   glproxy::resolve("glBindRenderbuffer");
    auto rRBStor   = (PFN_RBStorage)glproxy::resolve("glRenderbufferStorage");
    auto rFBTex    = (PFN_FBTex2D)  glproxy::resolve("glFramebufferTexture2D");
    auto rFBRB     = (PFN_FBRB)     glproxy::resolve("glFramebufferRenderbuffer");
    auto rFBStat   = (PFN_FBStat)   glproxy::resolve("glCheckFramebufferStatus");
    raw_bindFB     = rBindFB;
    raw_viewport   = (PFN_Viewport) glproxy::resolve("glViewport");
    raw_blit       = (PFN_Blit)     glproxy::resolve("glBlitFramebuffer");
    raw_getIv      = (PFN_GetIv)    glproxy::resolve("glGetIntegerv");

    if (!rGenFB || !rBindFB || !rGenTex || !rBindTex || !rTexImg ||
        !rTexParam || !rGenRB || !rBindRB || !rRBStor || !rFBTex ||
        !rFBRB || !rFBStat || !raw_viewport || !raw_blit) {
        angle::log("halfres_render: required GL functions unavailable - disabled");
        return;
    }

    s_realW = realW;
    s_realH = realH;
    GLint halfW = realW / 2;
    GLint halfH = realH / 2;

    // 1. Color texture (half-res, RGBA8)
    rGenTex(1, &s_offColor);
    rBindTex(0x0DE1 /*GL_TEXTURE_2D*/, s_offColor);
    rTexImg(0x0DE1, 0, 0x1908 /*GL_RGBA*/, halfW, halfH, 0, 0x1908, 0x1401 /*UNSIGNED_BYTE*/, nullptr);
    rTexParam(0x0DE1, 0x2801 /*MIN_FILTER*/, 0x2601 /*LINEAR*/);
    rTexParam(0x0DE1, 0x2800 /*MAG_FILTER*/, 0x2601);
    rTexParam(0x0DE1, 0x2802 /*WRAP_S*/,    0x812F /*CLAMP_TO_EDGE*/);
    rTexParam(0x0DE1, 0x2803 /*WRAP_T*/,    0x812F);

    // 2. Depth-stencil renderbuffer
    rGenRB(1, &s_offDS);
    rBindRB(0x8D41 /*GL_RENDERBUFFER*/, s_offDS);
    rRBStor(0x8D41, 0x88F0 /*DEPTH24_STENCIL8*/, halfW, halfH);

    // 3. FBO with both attached
    rGenFB(1, &s_offFBO);
    rBindFB(0x8D40 /*GL_FRAMEBUFFER*/, s_offFBO);
    rFBTex(0x8D40, 0x8CE0 /*COLOR_ATTACHMENT0*/, 0x0DE1, s_offColor, 0);
    rFBRB(0x8D40,  0x821A /*DEPTH_STENCIL_ATTACHMENT*/, 0x8D41, s_offDS);

    GLenum status = rFBStat(0x8D40);
    if (status != 0x8CD5 /*GL_FRAMEBUFFER_COMPLETE*/) {
        angle::log("halfres_render: FBO incomplete (status=0x%04X) - disabled", status);
        // leave resources allocated but inactive; restoration of raw FB
        // happens in caller (wgl_proxy)
        rBindFB(0x8D40, 0);
        return;
    }

    // 4. Leave the offscreen FBO bound. From now on, all "render to FB 0"
    //    calls in cocos2d will be redirected by gl_glBindFramebuffer to here.
    s_active = true;
    angle::log("halfres_render: ACTIVE - real=%dx%d offscreen=%dx%d (FBO=%u)",
               realW, realH, halfW, halfH, s_offFBO);
}

// =====================================================================
// Per-call hook helpers (called from gl_glBindFramebuffer / gl_glViewport)
// =====================================================================

// Returns the FBO ID that should actually be bound. If the caller asked for
// FB 0 and halfres is active, return our offscreen FBO instead.
extern "C" GLuint gdangle_halfresRedirectFB(GLuint requested) {
    if (s_active && requested == 0) return s_offFBO;
    return requested;
}

// Returns whether the given viewport size matches the real backbuffer (and
// should be halved).
extern "C" int gdangle_halfresShouldHalveViewport(GLsizei w, GLsizei h) {
    return (s_active && w == s_realW && h == s_realH) ? 1 : 0;
}

extern "C" int gdangle_halfresGetSize(GLint* outW, GLint* outH) {
    if (!s_active) return 0;
    if (outW) *outW = s_realW;
    if (outH) *outH = s_realH;
    return 1;
}

// =====================================================================
// Pre-Present blit - called from wgl_proxy.cpp
// =====================================================================
extern "C" void gdangle_halfresPrePresent() {
    if (!s_active) return;
    if (!raw_bindFB || !raw_blit || !raw_viewport) return;

    GLint halfW = s_realW / 2;
    GLint halfH = s_realH / 2;

    // Save current FBO bindings (so we can restore after Present)
    GLint prevReadFB = 0, prevDrawFB = 0;
    if (raw_getIv) {
        raw_getIv(0x8CAA /*READ_FRAMEBUFFER_BINDING*/, &prevReadFB);
        raw_getIv(0x8CA6 /*DRAW_FRAMEBUFFER_BINDING*/, &prevDrawFB);
    }

    // Set read FB = our offscreen, draw FB = real default (0)
    raw_bindFB(0x8CA8 /*READ_FRAMEBUFFER*/, s_offFBO);
    raw_bindFB(0x8CA9 /*DRAW_FRAMEBUFFER*/, 0);

    // Real-size viewport for the blit destination
    raw_viewport(0, 0, s_realW, s_realH);

    // Blit half -> full with linear filter
    raw_blit(0, 0, halfW, halfH,
             0, 0, s_realW, s_realH,
             0x00004000 /*GL_COLOR_BUFFER_BIT*/, 0x2601 /*LINEAR*/);

    // Restore previous bindings (cocos2d's next frame expects FBO bound to
    // our offscreen FBO via the hook, but explicit restore is harmless).
    if (prevReadFB) raw_bindFB(0x8CA8, (GLuint)prevReadFB);
    if (prevDrawFB) raw_bindFB(0x8CA9, (GLuint)prevDrawFB);
}

// =====================================================================
// Public apply() - just logs intent. Real init happens via gdangle_halfresInit()
// from wgl_proxy after eglMakeCurrent succeeds.
// =====================================================================
void apply() {
    if (Config::get().halfres_render) {
        angle::log("halfres_render: opt-in flag set, will init after GL context up");
    }
}

} // namespace
