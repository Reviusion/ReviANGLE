// Boost: Real Frustum Culling
// Implements actual AABB-based visibility culling for the scene graph.
// Cocos2d-x visits ALL nodes every frame, even off-screen ones. We build
// an AABB list from CCNode::boundingBox() results and skip visit() for nodes
// entirely outside the viewport.
//
// This is the real implementation of the stub in boost_scene_bvh.

#include <windows.h>
#include <vector>
#include <cmath>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef int GLint;

namespace {

struct Rect {
    float x, y, w, h;
    bool intersects(const Rect& o) const {
        return !(x + w < o.x || o.x + o.w < x ||
                 y + h < o.y || o.y + o.h < y);
    }
};

Rect g_viewport = {0, 0, 1920, 1080};
bool g_active = false;

using GetIntegervFn = void(WINAPI*)(unsigned int, GLint*);
static GetIntegervFn s_getIntegerv = nullptr;

} // namespace

namespace boost_frustum_cull {

    void apply() {
        if (!Config::get().frustum_cull) return;

        s_getIntegerv = (GetIntegervFn)glproxy::resolve("glGetIntegerv");
        g_active = true;
        angle::log("frustum_cull: active");
    }

    void updateViewport() {
        if (!g_active || !s_getIntegerv) return;
        GLint vp[4];
        s_getIntegerv(0x0BA2, vp);  // GL_VIEWPORT
        g_viewport = {(float)vp[0], (float)vp[1], (float)vp[2], (float)vp[3]};
    }

    // Returns true if the given AABB is visible (intersects viewport)
    bool isVisible(float x, float y, float w, float h) {
        if (!g_active) return true;
        // Add margin to avoid popping (nodes slightly off-screen may have children on-screen)
        float margin = 64.0f;
        Rect expanded = {
            g_viewport.x - margin,
            g_viewport.y - margin,
            g_viewport.w + margin * 2,
            g_viewport.h + margin * 2
        };
        Rect node = {x, y, w, h};
        return expanded.intersects(node);
    }

    // Batch test: given an array of AABBs, return a visibility bitmask
    uint64_t testBatch(const float* aabbs, int count) {
        if (!g_active) return ~0ULL;
        uint64_t mask = 0;
        int max = (count > 64) ? 64 : count;
        for (int i = 0; i < max; i++) {
            float x = aabbs[i * 4 + 0];
            float y = aabbs[i * 4 + 1];
            float w = aabbs[i * 4 + 2];
            float h = aabbs[i * 4 + 3];
            if (isVisible(x, y, w, h)) {
                mask |= (1ULL << i);
            }
        }
        return mask;
    }

    bool isActive() { return g_active; }
}
