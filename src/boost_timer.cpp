// Boost: Timer resolution
// Windows default timer tick is 15.6ms; games that Sleep() or WaitForX
// suffer jitter. timeBeginPeriod(1) forces 1ms. Valid until DLL unload.

#include <windows.h>
#include <timeapi.h>
#include "config.hpp"

static bool g_timerRaised = false;

namespace boost_timer {
    void apply() {
        if (!Config::get().timer_fix) return;
        if (timeBeginPeriod(1) == TIMERR_NOERROR) {
            g_timerRaised = true;
        }
    }

    void restore() {
        if (g_timerRaised) {
            timeEndPeriod(1);
            g_timerRaised = false;
        }
    }
}
