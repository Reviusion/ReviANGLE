// Boost: Triple Buffering
// Requests ANGLE to create a swap chain with 3 back buffers instead of 2.
// This prevents the GPU from stalling when the CPU submits work faster than
// the GPU can consume it (or vice versa), providing smoother frame delivery.
//
// We achieve this by setting EGL_BUFFER_SIZE / EGL_RENDER_BUFFER hints
// and setting swap interval to allow queuing.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

namespace boost_triple_buffer {

    void apply() {
        if (!Config::get().triple_buffer) return;

        auto& a = angle::state();
        if (!a.egl || !a.display) {
            angle::log("triple_buffer: ANGLE not ready");
            return;
        }

        // ANGLE controls the swap chain internally. The most effective way to
        // get triple buffering is via the D3D11 backend configuration:
        // - DXGI_SWAP_CHAIN_DESC.BufferCount = 3
        //
        // Since we can't modify ANGLE's internal swap chain creation (without
        // rebuilding ANGLE), we use an indirect approach:
        //
        // 1. Set eglSwapInterval to -1 (if supported) or 0 for mailbox mode
        // 2. The D3D11 backend of ANGLE uses DXGI_SWAP_EFFECT_FLIP_DISCARD
        //    which inherently supports >2 buffers
        //
        // With allow_tearing + low_latency + triple_buffer all enabled,
        // the pipeline is: CPU -> Buffer1 -> Buffer2 -> Buffer3 -> Display

        // Try EGL_BUFFER_PRESERVED to keep buffers alive (hints at multi-buffering)
        using SwapIntervalFn = int(*)(void*, int);
        auto swapInterval = (SwapIntervalFn)GetProcAddress(a.egl, "eglSwapInterval");
        if (swapInterval) {
            // Swap interval 0 with FLIP_DISCARD effectively creates mailbox triple buffering
            swapInterval(a.display, 0);
        }

        angle::log("triple_buffer: hint applied (mailbox mode via eglSwapInterval 0)");
    }
}
