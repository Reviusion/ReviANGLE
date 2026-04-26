#pragma once

namespace boost_allow_tearing {
    void apply();
    bool isEnabled();           // true if ALLOW_TEARING flag is being injected
    bool isTearingSupported();  // DXGI 1.5 + driver actually supports it
}
