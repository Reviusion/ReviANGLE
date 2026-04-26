// Boost: thread priority + CPU affinity
//
// Sets the GD process to ABOVE_NORMAL priority and the calling thread to HIGHEST.
// Optionally pins the main thread to specific CPU cores if a mask is configured.

#include <windows.h>
#include "config.hpp"

namespace boost_thread {

    void apply() {
        auto& cfg = Config::get();
        if (!cfg.thread_boost) return;

        HANDLE proc = GetCurrentProcess();
        SetPriorityClass(proc, ABOVE_NORMAL_PRIORITY_CLASS);

        HANDLE thr = GetCurrentThread();
        SetThreadPriority(thr, THREAD_PRIORITY_HIGHEST);

        if (cfg.cpu_affinity != 0) {
            SetThreadAffinityMask(thr, (DWORD_PTR)cfg.cpu_affinity);
        }
    }
}
