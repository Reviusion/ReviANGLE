#include "angle_loader.hpp"
#include "config.hpp"
#include <cstdio>
#include <cstdarg>
#include <mutex>

// EGL constants we need (copied from EGL/egl.h to avoid header dependency)
constexpr EGLint_t EGL_PLATFORM_ANGLE_ANGLE             = 0x3202;
constexpr EGLint_t EGL_PLATFORM_ANGLE_TYPE_ANGLE        = 0x3203;
constexpr EGLint_t EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE  = 0x3208;
constexpr EGLint_t EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE   = 0x3207;
constexpr EGLint_t EGL_DEFAULT_DISPLAY                  = 0;
constexpr EGLint_t EGL_NONE                             = 0x3038;

static angle::Loaded g_state;
static std::mutex    g_logMutex;
static FILE*         g_logFile      = nullptr;
static bool          g_logOpenTried = false;

namespace angle {

Loaded& state() { return g_state; }

// Persistent log file: opened once, then reused for every angle::log() call.
// Eliminates the per-call fopen+fclose hitch (~5–15 ms on NTFS due to metadata
// journal + AV scan) which was a primary microfreeze source on the hot path.
// Buffer is fully buffered (8 KB); flushed on process exit via atexit handler.
static void closeLogOnExit() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) {
        std::fflush(g_logFile);
        std::fclose(g_logFile);
        g_logFile = nullptr;
    }
}

void log(const char* fmt, ...) {
    auto& cfg = Config::get();
    if (!cfg.debug) return;

    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!g_logFile && !g_logOpenTried) {
        g_logOpenTried = true;
        g_logFile = std::fopen(cfg.log_file.c_str(), "a");
        if (g_logFile) {
            // Full buffering — CRT flushes only when buffer fills (~50+ lines).
            std::setvbuf(g_logFile, nullptr, _IOFBF, 8192);
            std::atexit(&closeLogOnExit);
        }
    }
    if (!g_logFile) return;

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(g_logFile, fmt, ap);
    va_end(ap);
    std::fputc('\n', g_logFile);
    // No fclose per call. fflush every 8 lines — pure WriteFile to the OS
    // write cache, no metadata journal, costs <0.1 ms. Guarantees logs
    // survive Task Manager / Kill() and most hard exits.
    // Threshold of 8 keeps init/error traces fresh on disk (any GLFW/EGL
    // failure dumps several lines, all flushed within ~one log call).
    static int s_flushCounter = 0;
    if (++s_flushCounter >= 8) {
        std::fflush(g_logFile);
        s_flushCounter = 0;
    }
}

static bool loadSymbols() {
    #define L(name) \
        g_state.name = (decltype(g_state.name))GetProcAddress(g_state.egl, #name); \
        if (!g_state.name) { log("missing export: " #name); return false; }

    L(eglGetDisplay);
    L(eglInitialize);
    L(eglChooseConfig);
    L(eglCreateWindowSurface);
    L(eglCreateContext);
    L(eglMakeCurrent);
    L(eglSwapBuffers);
    L(eglSwapInterval);
    L(eglDestroyContext);
    L(eglDestroySurface);
    L(eglTerminate);
    L(eglGetProcAddress);
    L(eglGetError);

    // optional, may not exist on older ANGLE
    g_state.eglGetPlatformDisplayEXT = (decltype(g_state.eglGetPlatformDisplayEXT))
        GetProcAddress(g_state.egl, "eglGetPlatformDisplayEXT");

    #undef L
    return true;
}

static EGLDisplay_t openDisplayWithBackend(const std::string& backend) {
    if (!g_state.eglGetPlatformDisplayEXT) {
        log("eglGetPlatformDisplayEXT not available, using default display");
        return g_state.eglGetDisplay((void*)(uintptr_t)EGL_DEFAULT_DISPLAY);
    }

    EGLint_t platformType = EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE;
    if (backend == "d3d9") {
        platformType = EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE;
    }

    EGLint_t attribs[] = {
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, platformType,
        EGL_NONE, EGL_NONE
    };

    return g_state.eglGetPlatformDisplayEXT(
        EGL_PLATFORM_ANGLE_ANGLE,
        (void*)(uintptr_t)EGL_DEFAULT_DISPLAY,
        attribs
    );
}

bool init() {
    if (g_state.initialized) return true;

    auto& cfg = Config::get();
    log("ReviANGLE init, requested backend: %s", cfg.backend.c_str());

    g_state.egl   = LoadLibraryA("libEGL.dll");
    g_state.gles2 = LoadLibraryA("libGLESv2.dll");

    if (!g_state.egl || !g_state.gles2) {
        log("failed to load ANGLE DLLs (egl=%p gles2=%p)", g_state.egl, g_state.gles2);
        return false;
    }

    if (!loadSymbols()) {
        log("failed to load EGL symbols");
        return false;
    }

    g_state.display = openDisplayWithBackend(cfg.backend);
    if (!g_state.display && cfg.backend != "d3d9") {
        log("primary backend failed, trying d3d9 fallback");
        g_state.display = openDisplayWithBackend("d3d9");
    }

    if (!g_state.display) {
        log("could not get EGL display at all");
        return false;
    }

    EGLint_t major = 0, minor = 0;
    if (!g_state.eglInitialize(g_state.display, &major, &minor)) {
        log("eglInitialize failed: 0x%x", g_state.eglGetError());
        return false;
    }

    log("EGL initialized %d.%d", major, minor);
    g_state.initialized = true;
    return true;
}

void shutdown() {
    if (!g_state.initialized) return;
    if (g_state.display && g_state.eglTerminate) {
        g_state.eglTerminate(g_state.display);
    }
    if (g_state.gles2) FreeLibrary(g_state.gles2);
    if (g_state.egl)   FreeLibrary(g_state.egl);
    g_state = {};
}

} // namespace angle
