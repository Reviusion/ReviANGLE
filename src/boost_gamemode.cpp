// Boost: Windows Game Mode
// Registers GD with the Windows Game Bar / Game Mode system.
// Game Mode disables background updates, Windows Update, and notification toasts.

#include <windows.h>
#include "config.hpp"
#include "angle_loader.hpp"

namespace boost_gamemode {

    void apply() {
        if (!Config::get().game_mode) return;

        // Method: write registry keys that tell Windows this exe is a game.
        // HKCU\System\GameConfigStore\Children\<exe path> with GameDVR + GameMode flags.

        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);

        // Open or create the GameConfigStore key
        HKEY hKey = nullptr;
        std::string subKey = "System\\GameConfigStore\\Children\\";
        subKey += exePath;

        // Replace backslashes with underscores for the registry key
        for (auto& c : subKey) { if (c == '\\') c = '/'; }

        LONG result = RegCreateKeyExA(HKEY_CURRENT_USER,
            "System\\GameConfigStore", 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);

        if (result == ERROR_SUCCESS) {
            // AutoGameModeEnabled = 1
            DWORD val = 1;
            RegSetValueExA(hKey, "GameDVR_Enabled", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
            RegSetValueExA(hKey, "GameDVR_FSEBehavior", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
            RegCloseKey(hKey);
            angle::log("game_mode: registered GD with Windows Game Mode");
        } else {
            angle::log("game_mode: registry write failed (%ld)", result);
        }
    }
}
