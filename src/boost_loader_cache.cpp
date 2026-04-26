// Boost: GetProcAddress cache
// GD / cocos2d calls GetProcAddress thousands of times at startup.
// We cache results in a hash map so repeat lookups are O(1).

#include <windows.h>
#include <unordered_map>
#include <string>
#include <mutex>
#include "config.hpp"
#include "common/iat_hook.hpp"
#include "angle_loader.hpp"

struct ProcKey {
    HMODULE mod;
    std::string name;
    bool operator==(const ProcKey& o) const { return mod == o.mod && name == o.name; }
};

struct ProcKeyHash {
    size_t operator()(const ProcKey& k) const {
        auto h1 = std::hash<void*>()((void*)k.mod);
        auto h2 = std::hash<std::string>()(k.name);
        return h1 ^ (h2 << 1);
    }
};

static std::unordered_map<ProcKey, FARPROC, ProcKeyHash> g_cache;
static std::mutex g_mu;

using GetProcAddressFn = FARPROC(WINAPI*)(HMODULE, LPCSTR);
static GetProcAddressFn s_origGetProcAddress = nullptr;

static FARPROC WINAPI hooked_GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    // ordinal imports don't have a name string
    if ((uintptr_t)lpProcName <= 0xFFFF) {
        return s_origGetProcAddress(hModule, lpProcName);
    }

    ProcKey key{hModule, lpProcName};
    {
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = g_cache.find(key);
        if (it != g_cache.end()) return it->second;
    }

    FARPROC result = s_origGetProcAddress(hModule, lpProcName);

    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_cache[key] = result;
    }
    return result;
}

namespace boost_loader_cache {

    void apply() {
        if (!Config::get().loader_cache) return;

        s_origGetProcAddress = (GetProcAddressFn)iat::hookInMainExe(
            "kernel32.dll", "GetProcAddress", (void*)hooked_GetProcAddress);

        if (s_origGetProcAddress) {
            angle::log("loader_cache: active");
        }
    }
}
