// Boost: fast I/O
// Hooks CreateFileA to add FILE_FLAG_SEQUENTIAL_SCAN for read-only opens.
// This tells Windows to prefetch ahead aggressively, improving sequential reads
// of textures, plists, and level data.

#include <windows.h>
#include "config.hpp"
#include "common/iat_hook.hpp"
#include "angle_loader.hpp"

using CreateFileAFn = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static CreateFileAFn s_origCreateFileA = nullptr;

static HANDLE WINAPI hooked_CreateFileA(
    LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSA, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplate)
{
    // only modify read-only opens for existing files
    if (dwDesiredAccess == GENERIC_READ &&
        dwCreationDisposition == OPEN_EXISTING &&
        !(dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED))
    {
        dwFlagsAndAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;
    }
    return s_origCreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                             lpSA, dwCreationDisposition, dwFlagsAndAttributes, hTemplate);
}

namespace boost_fast_io {

    void apply() {
        if (!Config::get().fast_io) return;

        s_origCreateFileA = (CreateFileAFn)iat::hookInMainExe(
            "kernel32.dll", "CreateFileA", (void*)hooked_CreateFileA);

        if (s_origCreateFileA) {
            angle::log("fast_io: active (SEQUENTIAL_SCAN on all reads)");
        } else {
            angle::log("fast_io: IAT hook failed");
        }
    }
}
