#pragma once
#include <windows.h>

// Lightweight import address table hook utility.
// Useful for swapping CRT / kernel32 functions that a loaded module imports.

namespace iat {

    // Hook `funcName` imported from `dllName` inside the module at `moduleBase`.
    // Returns the original pointer (or nullptr if not found / not hooked).
    void* hook(HMODULE moduleBase, const char* dllName, const char* funcName, void* newFn);

    // Convenience: hook an import in the main .exe module.
    void* hookInMainExe(const char* dllName, const char* funcName, void* newFn);

} // namespace iat
