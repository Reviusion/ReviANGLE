// Boost: precise sleep
// Replaces Windows Sleep() with a high-resolution spin-wait + WaitableTimer
// for short durations. This eliminates the ~15ms jitter in frame pacing.

#include <windows.h>
#include "config.hpp"
#include "common/iat_hook.hpp"
#include "angle_loader.hpp"

using SleepFn = void (WINAPI*)(DWORD);
static SleepFn s_origSleep = nullptr;
static HANDLE  s_timer     = nullptr;

static void WINAPI preciseSleep(DWORD ms) {
    if (ms == 0) {
        SwitchToThread();
        return;
    }

    if (ms <= 2) {
        // spin wait using QPC for very short sleeps
        LARGE_INTEGER freq, start, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        double target = (double)ms * 0.001 * freq.QuadPart;
        do {
            SwitchToThread();
            QueryPerformanceCounter(&now);
        } while ((double)(now.QuadPart - start.QuadPart) < target);
        return;
    }

    // for longer sleeps, use a waitable timer (1ms resolution with timeBeginPeriod)
    if (s_timer) {
        LARGE_INTEGER due;
        due.QuadPart = -(LONGLONG)ms * 10000LL;  // 100ns units, negative = relative
        SetWaitableTimer(s_timer, &due, 0, nullptr, nullptr, FALSE);
        WaitForSingleObject(s_timer, ms + 10);
    } else {
        if (s_origSleep) s_origSleep(ms);
    }
}

namespace boost_sleep {

    void apply() {
        if (!Config::get().precise_sleep) return;

        s_timer = CreateWaitableTimerA(nullptr, TRUE, nullptr);
        s_origSleep = (SleepFn)iat::hookInMainExe("kernel32.dll", "Sleep", (void*)preciseSleep);

        if (s_origSleep) {
            angle::log("precise_sleep: active (IAT hooked Sleep)");
        } else {
            angle::log("precise_sleep: IAT hook failed");
        }
    }

    void shutdown() {
        if (s_timer) { CloseHandle(s_timer); s_timer = nullptr; }
    }
}
