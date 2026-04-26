// Boost: MMCSS Pro Audio thread scheduling
// Registers the main thread with the Multimedia Class Scheduler Service
// under the "Pro Audio" task, giving it guaranteed 1ms scheduling intervals
// and priority above most system processes.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

// avrt.dll functions (loaded dynamically to avoid hard dependency)
using AvSetMmThreadFn = HANDLE(WINAPI*)(LPCSTR, LPDWORD);
using AvRevertMmThreadFn = BOOL(WINAPI*)(HANDLE);

static HANDLE  g_mmcssHandle = nullptr;
static HMODULE g_avrt = nullptr;
static AvRevertMmThreadFn s_revert = nullptr;

namespace boost_mmcss {

    void apply() {
        if (!Config::get().mmcss_pro_audio) return;

        g_avrt = LoadLibraryA("avrt.dll");
        if (!g_avrt) {
            angle::log("mmcss: avrt.dll not found");
            return;
        }

        auto setThread = (AvSetMmThreadFn)GetProcAddress(g_avrt, "AvSetMmThreadCharacteristicsA");
        s_revert = (AvRevertMmThreadFn)GetProcAddress(g_avrt, "AvRevertMmThreadCharacteristics");

        if (!setThread) {
            angle::log("mmcss: AvSetMmThreadCharacteristicsA not found");
            FreeLibrary(g_avrt);
            g_avrt = nullptr;
            return;
        }

        DWORD taskIndex = 0;
        g_mmcssHandle = setThread("Pro Audio", &taskIndex);
        if (g_mmcssHandle) {
            angle::log("mmcss: main thread registered as Pro Audio (task=%lu)", taskIndex);
        } else {
            angle::log("mmcss: registration failed (err=%lu)", GetLastError());
        }
    }

    void shutdown() {
        if (g_mmcssHandle && s_revert) {
            s_revert(g_mmcssHandle);
            g_mmcssHandle = nullptr;
        }
        if (g_avrt) {
            FreeLibrary(g_avrt);
            g_avrt = nullptr;
        }
    }
}
