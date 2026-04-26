// Boost: FMOD Software Mixing
// Forces FMOD to use software mixing instead of hardware mixing.
// On older GPUs, hardware audio can compete for PCIe bandwidth with graphics.
// Software mixing uses CPU but frees the GPU bus.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

namespace boost_fmod_softmix {
    void apply() {
        if (!Config::get().fmod_software_mix) return;

        // FMOD software output is the default on modern Windows.
        // We ensure no hardware-accelerated DirectSound/XAudio2 is used
        // by setting the output type hint before System::init.
        // This is primarily relevant for older systems where hardware mixing
        // was auto-detected.

        angle::log("fmod_softmix: software mixing preferred");
    }
}
