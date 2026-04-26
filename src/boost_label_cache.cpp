// Boost: BMFont Label Cache
// CCLabelBMFont recalculates glyph positions every frame even when text
// hasn't changed. We cache the vertex data keyed by (text + font + size)
// and skip recalculation for unchanged labels.

#include <windows.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include "config.hpp"
#include "angle_loader.hpp"

namespace {

struct CachedLabel {
    std::string text;
    std::vector<float> vertices;  // cached vertex positions
    uint32_t hash;
};

std::unordered_map<uintptr_t, CachedLabel> g_cache;
std::mutex g_mu;
bool g_active = false;

uint32_t hashString(const char* s) {
    uint32_t h = 5381;
    while (*s) { h = ((h << 5) + h) + (unsigned char)*s++; }
    return h;
}

} // namespace

namespace boost_label_cache {
    void apply() {
        if (!Config::get().label_cache) return;
        g_cache.reserve(256);
        g_active = true;
        angle::log("label_cache: active");
    }

    bool lookup(uintptr_t labelPtr, const char* text, const float** outVerts, int* outCount) {
        if (!g_active || !text) return false;
        uint32_t h = hashString(text);

        std::lock_guard<std::mutex> lk(g_mu);
        auto it = g_cache.find(labelPtr);
        if (it != g_cache.end() && it->second.hash == h) {
            *outVerts = it->second.vertices.data();
            *outCount = (int)it->second.vertices.size();
            return true;
        }
        return false;
    }

    void store(uintptr_t labelPtr, const char* text, const float* verts, int count) {
        if (!g_active || !text || !verts) return;
        std::lock_guard<std::mutex> lk(g_mu);
        auto& entry = g_cache[labelPtr];
        entry.text = text;
        entry.hash = hashString(text);
        entry.vertices.assign(verts, verts + count);
    }

    void invalidate(uintptr_t labelPtr) {
        if (!g_active) return;
        std::lock_guard<std::mutex> lk(g_mu);
        g_cache.erase(labelPtr);
    }
}
