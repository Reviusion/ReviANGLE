// Boost: FMOD Channel Limit
// GD initializes FMOD with 32 channels. Most levels only use 4-8.
// Extra channels consume CPU for mixing silence. We limit to 16.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

namespace boost_fmod_channels {
    static int g_maxChannels = 16;

    void apply() {
        auto& cfg = Config::get();
        if (!cfg.fmod_channel_limit) return;
        g_maxChannels = cfg.fmod_max_channels;
        if (g_maxChannels < 4) g_maxChannels = 4;
        if (g_maxChannels > 64) g_maxChannels = 64;

        // FMOD channel limit is set during System::init which has already been
        // called by the time our DLL loads. The effective way is to hook
        // FMOD::System::init and pass our maxChannels.
        // For proxy DLL approach, we log the intent and the FMOD tuning module
        // (boost_fmod) handles the actual limiting.
        angle::log("fmod_channels: target=%d channels", g_maxChannels);
    }

    int getMaxChannels() { return g_maxChannels; }
}
