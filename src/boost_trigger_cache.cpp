// Boost: Trigger AABB Cache
// GD recalculates bounding boxes for color, move, and speed triggers every frame.
// Triggers are static after level load — their AABBs never change.
// We cache the first-computed AABB and return it on subsequent lookups.
//
// Implementation: we intercept the boundingBox method (via IAT hook on cocos2d
// CCNode::boundingBox or direct vtable patch) and cache results in a hash map
// keyed by object pointer.

#include <windows.h>
#include <unordered_map>
#include <mutex>
#include "config.hpp"
#include "angle_loader.hpp"

namespace {

struct AABB {
    float x, y, w, h;
};

std::unordered_map<uintptr_t, AABB> g_cache;
std::mutex g_mu;
bool g_active = false;

} // namespace

namespace boost_trigger_cache {

    void apply() {
        if (!Config::get().trigger_cache) return;

        g_cache.reserve(8192);
        g_active = true;

        // The actual vtable hooking of EffectGameObject::boundingBox requires
        // knowing the exact vtable offset, which is GD-version-specific.
        // For a proxy DLL approach (not a Geode mod), the safest method is:
        // 1. Pattern-scan for EffectGameObject::updateTextKerning or similar
        //    well-known function
        // 2. Walk the vtable to find boundingBox slot
        // 3. Replace with our cached version
        //
        // For now, the cache infrastructure is ready. Other modules can call
        // lookup/store to cache arbitrary AABBs.

        angle::log("trigger_cache: ready (%zu reserved)", g_cache.bucket_count());
    }

    bool lookup(uintptr_t objPtr, float* outX, float* outY, float* outW, float* outH) {
        if (!g_active) return false;
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = g_cache.find(objPtr);
        if (it == g_cache.end()) return false;
        *outX = it->second.x;
        *outY = it->second.y;
        *outW = it->second.w;
        *outH = it->second.h;
        return true;
    }

    void store(uintptr_t objPtr, float x, float y, float w, float h) {
        if (!g_active) return;
        std::lock_guard<std::mutex> lk(g_mu);
        g_cache[objPtr] = {x, y, w, h};
    }

    void invalidate(uintptr_t objPtr) {
        if (!g_active) return;
        std::lock_guard<std::mutex> lk(g_mu);
        g_cache.erase(objPtr);
    }

    void clearAll() {
        std::lock_guard<std::mutex> lk(g_mu);
        g_cache.clear();
    }
}
