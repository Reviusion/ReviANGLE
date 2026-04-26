// ReviANGLE proxy opengl32.dll entry point.
// Initialises config, applies all boost modules, defers ANGLE until wglCreateContext.
//
// ReviANGLE  —  Performance Suite for Geometry Dash  by Reviusion

#include <windows.h>
#include <cstdio>
#include "config.hpp"
#include "angle_loader.hpp"
#include "gl_proxy.hpp"

// base
namespace boost_gpu    { void apply(); }
namespace boost_timer  { void apply(); void restore(); }
namespace boost_thread { void apply(); }
namespace boost_power  { void apply(); }
namespace boost_alloc  { void apply(); }
namespace boost_math   { void apply(); }

// advanced
namespace boost_tex_compress  { void apply(); }
namespace boost_nvapi         { void apply(); }
namespace boost_shader_cache  { void apply(); }
namespace boost_laa           { void apply(); }
namespace boost_gl_dedup      { void apply(); }
namespace boost_prefetch      { void apply(); }
namespace boost_fmod          { void apply(); }
namespace boost_asset_loader  { void apply(); void shutdown(); }
namespace boost_vsync         { void apply(); }
namespace boost_sleep         { void apply(); void shutdown(); }
namespace boost_heap          { void apply(); void shutdown(); }
namespace boost_d3d11_mt      { void apply(); }

// render
namespace boost_depth_off        { void apply(); }
namespace boost_mipmap_off       { void apply(); }
namespace boost_vbo_pool         { void apply(); }
namespace boost_vertex_compress  { void apply(); }
namespace boost_instancing       { void apply(); }
namespace boost_dyn_res          { void apply(); }

// IO
namespace boost_fast_io        { void apply(); }
namespace boost_ramdisk        { void apply(); void shutdown(); }
namespace boost_loader_cache   { void apply(); }

// CPU
namespace boost_sse_memcpy     { void apply(); }
namespace boost_scene_bvh      { void apply(); }
namespace boost_string_intern  { void apply(); }
namespace boost_mimalloc_full  { void apply(); }
namespace boost_silent         { void apply(); }

// system
namespace boost_wddm_prio     { void apply(); }
namespace boost_gamemode       { void apply(); }
namespace boost_smart_pin      { void apply(); }
namespace boost_mitigation_off { void apply(); }

// anti-stutter
namespace boost_allow_tearing  { void apply(); }
namespace boost_waitable_swap  { void apply(); }
namespace boost_frame_pacing   { void apply(); void shutdown(); }
namespace boost_mmcss          { void apply(); void shutdown(); }
namespace boost_shader_warmup  { void apply(); }
namespace boost_low_latency    { void apply(); }

// GD-specific
namespace boost_skip_intro       { void apply(); }
namespace boost_obj_pool         { void apply(); void shutdown(); }
namespace boost_trigger_cache    { void apply(); }
namespace boost_plist_bin        { void apply(); }
namespace boost_skip_effects     { void apply(); }
namespace boost_unlock_fps       { void apply(); void reapply(); bool isActive(); }
namespace boost_level_predecode  { void apply(); void shutdown(); }
namespace boost_anti_stutter     { void apply(); }

// advanced render
namespace boost_atlas_merge    { void apply(); }
namespace boost_frustum_cull   { void apply(); }
namespace boost_fbo_cache      { void apply(); void shutdown(); }
namespace boost_triple_buffer  { void apply(); }
namespace boost_no_aa          { void apply(); }
namespace boost_blend_opt      { void apply(); }

// cocos2d-x
namespace boost_particle_throttle { void apply(); }
namespace boost_texcache_preload  { void apply(); }
namespace boost_batch_force       { void apply(); }
namespace boost_label_cache       { void apply(); }
namespace boost_scheduler_skip    { void apply(); }
namespace boost_drawcall_sort     { void apply(); }
namespace boost_index_gen         { void apply(); }

// system advanced
namespace boost_ftz_daz       { void apply(); }
namespace boost_spectre_off   { void apply(); }
namespace boost_io_priority   { void apply(); }
namespace boost_mem_priority  { void apply(); }
namespace boost_stack_trim    { void apply(); }

// pipeline
namespace boost_drawsort        { void apply(); }
namespace boost_scissor_tight   { void apply(); }
namespace boost_vertex_dedup    { void apply(); }
namespace boost_halfres_fx      { void apply(); }
namespace boost_shader_simplify { void apply(); }
namespace boost_batch_coalesce  { void apply(); }

// network
namespace boost_dns_prefetch  { void apply(); }
namespace boost_http_pool     { void apply(); }
namespace boost_online_block  { void apply(); }
namespace boost_server_cache  { void apply(); }
namespace boost_winsock_opt   { void apply(); }

// audio
namespace boost_fmod_channels   { void apply(); }
namespace boost_fmod_softmix    { void apply(); }
namespace boost_audio_pin       { void apply(); }
namespace boost_sound_preload   { void apply(); }
namespace boost_audio_compress  { void apply(); }

// extreme
namespace boost_etw_off         { void apply(); }
namespace boost_wer_off         { void apply(); }
namespace boost_smartscreen_off { void apply(); }
namespace boost_numa            { void apply(); }
namespace boost_huge_pages      { void apply(); }
namespace boost_prefetcher_off  { void apply(); }

// Windows-level extras
namespace boost_workingset_lock { void apply(); }

// opt-in idle skip + halfres rendering
namespace boost_present_skip   { void apply(); }
namespace boost_halfres_render { void apply(); }

static void onAttach(HMODULE self) {
    DisableThreadLibraryCalls(self);
    Config::get().load("angle_config.ini");

    // ---- base modules (safe, no GL dependency) ----
    boost_gpu::apply();
    boost_timer::apply();
    boost_thread::apply();
    boost_power::apply();
    boost_alloc::apply();
    boost_math::apply();

    // ---- advanced (system-level, no GL) ----
    boost_laa::apply();
    // boost_nvapi::apply() — moved to gdangle_postGLInit() (see below).
    // Calling LoadLibraryA("nvapi64.dll") from DllMain re-enters the loader
    // lock; nvapi64.dll's own DllMain pulls in heavy Nvidia driver DLLs
    // (nvldumdx.dll, etc.), some of which fail under nested loader-lock
    // and abort the entire DLL load with STATUS_DLL_INIT_FAILED (0xC0000142).
    boost_prefetch::apply();
    boost_fmod::apply();
    boost_asset_loader::apply();
    boost_sleep::apply();
    boost_heap::apply();

    // ---- IO (hooks before GL init) ----
    boost_fast_io::apply();
    boost_ramdisk::apply();
    boost_loader_cache::apply();

    // ---- CPU ----
    boost_mimalloc_full::apply();
    boost_sse_memcpy::apply();
    boost_string_intern::apply();
    boost_silent::apply();
    boost_scene_bvh::apply();

    // ---- system ----
    boost_wddm_prio::apply();
    boost_gamemode::apply();
    boost_smart_pin::apply();
    boost_mitigation_off::apply();

    // ---- anti-stutter (system-level, no GL) ----
    boost_mmcss::apply();
    boost_anti_stutter::apply();   // disable affinity update + EcoQoS thread throttling

    // ---- GD-specific (IAT hooks, no GL) ----
    boost_skip_intro::apply();
    boost_obj_pool::apply();
    boost_trigger_cache::apply();
    boost_plist_bin::apply();
    boost_level_predecode::apply();

    // ---- system advanced (no GL) ----
    boost_ftz_daz::apply();
    boost_spectre_off::apply();
    boost_io_priority::apply();
    boost_mem_priority::apply();
    boost_stack_trim::apply();

    // ---- cocos2d-x (no GL) ----
    boost_texcache_preload::apply();
    boost_scheduler_skip::apply();

    // ---- network ----
    boost_dns_prefetch::apply();
    boost_http_pool::apply();
    boost_online_block::apply();
    boost_server_cache::apply();
    boost_winsock_opt::apply();

    // ---- audio ----
    boost_fmod_channels::apply();
    boost_fmod_softmix::apply();
    boost_audio_pin::apply();
    boost_sound_preload::apply();
    boost_audio_compress::apply();

    // ---- extreme ----
    boost_etw_off::apply();
    boost_wer_off::apply();
    boost_smartscreen_off::apply();
    boost_numa::apply();
    boost_huge_pages::apply();
    boost_prefetcher_off::apply();
    boost_workingset_lock::apply();

    // — opt-in optimizations
    boost_present_skip::apply();
    boost_halfres_render::apply();

    // ---- GL-dependent modules (deferred until context creation) ----
    // Called from wgl_wglMakeCurrent -> gdangle_postGLInit()

    angle::log("ReviANGLE attached \u2014 84 boost modules, backend=%s  (by Reviusion)",
               Config::get().backend.c_str());
}

// Called once after wglMakeCurrent succeeds — safe to touch GL now.
// Also: we are well outside the DLL loader lock here, so it's safe to
// LoadLibrary heavy DLLs like nvapi64.dll without risking 0xC0000142.
void gdangle_postGLInit() {
    // Diagnostic: log the *real* GPU ANGLE picked. On Optimus laptops this
    // confirms whether NvOptimusEnablement actually kicked the dGPU in.
    // Format from ANGLE: "ANGLE (NVIDIA, NVIDIA GeForce GT 630M Direct3D11 ...)"
    {
        HMODULE gles = GetModuleHandleA("libGLESv2.dll");
        if (gles) {
            using PFN_GetString = const char* (WINAPI*)(unsigned int);
            auto getStr = (PFN_GetString)GetProcAddress(gles, "glGetString");
            if (getStr) {
                const char* vendor   = getStr(0x1F00); // GL_VENDOR
                const char* renderer = getStr(0x1F01); // GL_RENDERER
                const char* version  = getStr(0x1F02); // GL_VERSION
                angle::log("GPU active: vendor='%s' renderer='%s' version='%s'",
                           vendor   ? vendor   : "?",
                           renderer ? renderer : "?",
                           version  ? version  : "?");
            }
        }
    }

    // Deferred from onAttach because nvapi64.dll's DllMain re-enters loader
    // lock and aborts process startup if loaded too early.
    boost_nvapi::apply();

    // GL-dependent
    boost_tex_compress::apply();
    boost_gl_dedup::apply();
    boost_shader_cache::apply();
    boost_d3d11_mt::apply();
    boost_depth_off::apply();
    boost_mipmap_off::apply();
    boost_vbo_pool::apply();
    boost_vertex_compress::apply();
    boost_instancing::apply();
    boost_dyn_res::apply();
    boost_vsync::apply();

    // anti-stutter (GL-dependent)
    boost_allow_tearing::apply();
    boost_waitable_swap::apply();
    boost_frame_pacing::apply();
    boost_shader_warmup::apply();
    boost_low_latency::apply();

    // GD-specific (GL-dependent)
    boost_skip_effects::apply();
    boost_unlock_fps::apply();   // remove cocos2d FPS cap (setAnimationInterval)

    // advanced render
    boost_atlas_merge::apply();
    boost_frustum_cull::apply();
    boost_fbo_cache::apply();
    boost_triple_buffer::apply();
    boost_no_aa::apply();
    boost_blend_opt::apply();

    // cocos2d-x (GL-dependent)
    boost_particle_throttle::apply();
    boost_batch_force::apply();
    boost_label_cache::apply();
    boost_drawcall_sort::apply();
    boost_index_gen::apply();

    // pipeline (GL-dependent)
    boost_drawsort::apply();
    boost_scissor_tight::apply();
    boost_vertex_dedup::apply();
    boost_halfres_fx::apply();
    boost_shader_simplify::apply();
    boost_batch_coalesce::apply();
}

static void onDetach() {
    boost_timer::restore();
    boost_sleep::shutdown();
    boost_heap::shutdown();
    boost_asset_loader::shutdown();
    boost_mmcss::shutdown();
    boost_obj_pool::shutdown();
    boost_level_predecode::shutdown();
    boost_fbo_cache::shutdown();
    boost_frame_pacing::shutdown();
    angle::shutdown();
}

// Unconditional load marker — proves our DLL was loaded
static void writeLoadMarker(HMODULE self, DWORD reason) {
    OutputDebugStringA("[ReviANGLE] DllMain DLL_PROCESS_ATTACH\n");

    // Write next to our own DLL (always writable since DLL was loaded from there)
    char dllPath[MAX_PATH] = {};
    GetModuleFileNameA(self, dllPath, MAX_PATH);
    char* slash = strrchr(dllPath, '\\');
    if (!slash) return;
    strcpy_s(slash + 1, MAX_PATH - (slash + 1 - dllPath), "gdangle_loaded.txt");

    FILE* f = nullptr;
    fopen_s(&f, dllPath, "a");
    if (!f) {
        // Fallback: temp dir
        char tmp[MAX_PATH] = {};
        GetTempPathA(MAX_PATH, tmp);
        strcat_s(tmp, "gdangle_loaded.txt");
        fopen_s(&f, tmp, "a");
        if (!f) return;
    }
    SYSTEMTIME st; GetLocalTime(&st);
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    fprintf(f, "[%02d:%02d:%02d.%03d] reason=%lu pid=%lu exe=%s dll=%s\n",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            reason, GetCurrentProcessId(), exePath, dllPath);
    fclose(f);
}

BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) writeLoadMarker(self, reason);
    switch (reason) {
        case DLL_PROCESS_ATTACH: onAttach(self); break;
        case DLL_PROCESS_DETACH: onDetach();     break;
    }
    return TRUE;
}
