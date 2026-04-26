// Boost: background heap compaction
// Runs HeapCompact periodically on a background thread to reduce memory
// fragmentation. Only activates when the GD window is NOT focused (to avoid
// hitching during gameplay).

#include <windows.h>
#include <thread>
#include <atomic>
#include "config.hpp"
#include "angle_loader.hpp"

static std::thread  g_thread;
static std::atomic<bool> g_stop{false};

static void heapWorker(int intervalSec) {
    while (!g_stop.load()) {
        for (int i = 0; i < intervalSec * 10 && !g_stop.load(); i++) {
            Sleep(100);
        }
        if (g_stop.load()) break;

        // only compact when not in foreground
        HWND fg = GetForegroundWindow();
        DWORD fgPid = 0;
        GetWindowThreadProcessId(fg, &fgPid);
        if (fgPid == GetCurrentProcessId()) continue;

        HeapCompact(GetProcessHeap(), 0);
    }
}

namespace boost_heap {

    void apply() {
        int interval = Config::get().heap_compact_interval;
        if (interval <= 0) return;

        g_stop.store(false);
        g_thread = std::thread(heapWorker, interval);
        g_thread.detach();
        angle::log("heap_compact: active, interval=%ds (background only)", interval);
    }

    void shutdown() {
        g_stop.store(true);
        // thread is detached, will exit on its own
    }
}
