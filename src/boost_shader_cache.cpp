// Boost: ANGLE shader disk cache
// ANGLE has a built-in blob cache callback (eglSetBlobCacheFuncsANDROID).
// We persist compiled D3D shaders on disk so subsequent launches skip
// shader compilation entirely.

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "config.hpp"
#include "angle_loader.hpp"

typedef void  (*EglSetBlobCacheFunc)(void*, void(*set)(const void*, long, const void*, long), long(*get)(const void*, long, void*, long));
typedef void  (*SetFn)(const void* key, long keySize, const void* value, long valueSize);
typedef long  (*GetFn)(const void* key, long keySize, void* value, long valueSize);

static std::string g_cacheDir;

static std::string keyToPath(const void* key, long keySize) {
    // hash the key bytes into a hex filename
    unsigned int hash = 5381;
    const auto* k = (const unsigned char*)key;
    for (long i = 0; i < keySize; i++) {
        hash = ((hash << 5) + hash) + k[i];
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%08x.bin", hash);
    return g_cacheDir + "\\" + buf;
}

static void WINAPI blobSet(const void* key, long keySize, const void* value, long valueSize) {
    auto path = keyToPath(key, keySize);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f) {
        std::fwrite(value, 1, (size_t)valueSize, f);
        std::fclose(f);
    }
}

static long WINAPI blobGet(const void* key, long keySize, void* value, long valueSize) {
    auto path = keyToPath(key, keySize);
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return 0;

    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (value && valueSize >= size) {
        std::fread(value, 1, (size_t)size, f);
    }
    std::fclose(f);
    return size;
}

namespace boost_shader_cache {

    void apply() {
        auto& cfg = Config::get();
        if (!cfg.shader_cache) return;

        g_cacheDir = cfg.shader_cache_dir;
        CreateDirectoryA(g_cacheDir.c_str(), nullptr);

        auto& a = angle::state();
        if (!a.egl) return;

        auto fn = (EglSetBlobCacheFunc)GetProcAddress(a.egl, "eglSetBlobCacheFuncsANDROID");
        if (!fn) {
            angle::log("shader_cache: eglSetBlobCacheFuncsANDROID not found in ANGLE");
            return;
        }

        fn(a.display, (void(*)(const void*, long, const void*, long))blobSet,
                       (long(*)(const void*, long, void*, long))blobGet);

        angle::log("shader_cache: active, dir=%s", g_cacheDir.c_str());
    }
}
