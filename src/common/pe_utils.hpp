#pragma once
#include <windows.h>

namespace pe {

    // Returns the NT headers of the given module (or the main exe if nullptr).
    PIMAGE_NT_HEADERS headers(HMODULE mod = nullptr);

    // Flips a Characteristics bit on the given module's FileHeader.
    // Returns true on success.
    bool setCharacteristic(HMODULE mod, WORD flag, bool on);

    // Returns the code section range (RVA start + size).
    bool codeRange(HMODULE mod, BYTE** start, SIZE_T* size);

} // namespace pe
