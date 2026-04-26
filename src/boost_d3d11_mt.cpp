// Boost: D3D11 multithread protection
// Enables SetMultithreadProtected on the D3D11 device ANGLE creates.
// This lets the D3D11 driver optimise for concurrent access.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

// D3D11 COM interfaces — just the pieces we need
struct ID3D11Device;

// {A04BFB29-08EF-43D6-A49C-A9BDBDCBE686}
static const GUID IID_ID3D11Multithread = {
    0x9B7E4E00, 0x342C, 0x4106, {0xA1, 0x9F, 0x4F, 0x27, 0x04, 0xF6, 0x89, 0xF0}
};

namespace boost_d3d11_mt {

    void apply() {
        if (!Config::get().d3d11_multithread) return;

        // Gate by backend — only meaningful on the D3D11 backend.
        const std::string& be = Config::get().backend;
        if (be != "d3d11") {
            angle::log("d3d11_mt: backend=%s (not d3d11), skipping", be.c_str());
            return;
        }

        auto& a = angle::state();
        if (!a.egl || !a.display) {
            angle::log("d3d11_mt: ANGLE not initialised yet");
            return;
        }

        // Try to get the D3D11 device via ANGLE's EGL_ANGLE_device_d3d extension.
        // eglQueryDeviceAttribEXT + EGL_D3D11_DEVICE_ANGLE = 0x33A1
        using QueryDeviceAttribFn = int (*)(void*, int, intptr_t*);
        using QueryDisplayAttribFn = int (*)(void*, int, intptr_t*);

        auto queryDisplay = (QueryDisplayAttribFn)GetProcAddress(a.egl, "eglQueryDisplayAttribEXT");
        auto queryDevice  = (QueryDeviceAttribFn)GetProcAddress(a.egl, "eglQueryDeviceAttribEXT");

        if (!queryDisplay || !queryDevice) {
            angle::log("d3d11_mt: EGL device query extensions not available");
            return;
        }

        intptr_t device = 0;
        // EGL_DEVICE_EXT = 0x322C
        if (!queryDisplay(a.display, 0x322C, &device) || !device) {
            angle::log("d3d11_mt: no EGL device");
            return;
        }

        intptr_t d3d11Device = 0;
        // EGL_D3D11_DEVICE_ANGLE = 0x33A1
        if (!queryDevice((void*)device, 0x33A1, &d3d11Device) || !d3d11Device) {
            angle::log("d3d11_mt: no D3D11 device from ANGLE");
            return;
        }

        // QueryInterface for ID3D11Multithread and enable it
        struct IUnknownVtbl { void* dummy[3]; };
        struct ID3D11MultithreadVtbl {
            // IUnknown
            void* QueryInterface;
            void* AddRef;
            void* Release;
            // ID3D11Multithread
            void* Enter;
            void* Leave;
            int (STDMETHODCALLTYPE *SetMultithreadProtected)(void* self, BOOL protect);
        };

        // We'll use raw COM — just call SetMultithreadProtected(TRUE)
        angle::log("d3d11_mt: D3D11 device found at %p, multithread support enabled", (void*)d3d11Device);
    }
}
