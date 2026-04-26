#include "iat_hook.hpp"
#include <cstring>
#include <cctype>

namespace iat {

static bool iequals(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (char)std::tolower((unsigned char)*a);
        char cb = (char)std::tolower((unsigned char)*b);
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == *b;
}

void* hook(HMODULE moduleBase, const char* dllName, const char* funcName, void* newFn) {
    if (!moduleBase || !dllName || !funcName || !newFn) return nullptr;

    auto base = (BYTE*)moduleBase;
    auto dos  = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

    auto nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    auto impDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (impDir.VirtualAddress == 0) return nullptr;

    auto imp = (PIMAGE_IMPORT_DESCRIPTOR)(base + impDir.VirtualAddress);

    for (; imp->Name; imp++) {
        const char* name = (const char*)(base + imp->Name);
        if (!iequals(name, dllName)) continue;

        auto* thunkOrig = (PIMAGE_THUNK_DATA)(base + imp->OriginalFirstThunk);
        auto* thunkIAT  = (PIMAGE_THUNK_DATA)(base + imp->FirstThunk);
        if (!imp->OriginalFirstThunk) thunkOrig = thunkIAT;

        for (; thunkOrig->u1.AddressOfData; thunkOrig++, thunkIAT++) {
            if (IMAGE_SNAP_BY_ORDINAL(thunkOrig->u1.Ordinal)) continue;

            auto by = (PIMAGE_IMPORT_BY_NAME)(base + thunkOrig->u1.AddressOfData);
            if (std::strcmp((const char*)by->Name, funcName) != 0) continue;

            DWORD oldProt = 0;
            if (!VirtualProtect(&thunkIAT->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProt))
                return nullptr;

            void* original = (void*)(uintptr_t)thunkIAT->u1.Function;
            thunkIAT->u1.Function = (uintptr_t)newFn;

            DWORD tmp;
            VirtualProtect(&thunkIAT->u1.Function, sizeof(void*), oldProt, &tmp);
            return original;
        }
    }
    return nullptr;
}

void* hookInMainExe(const char* dllName, const char* funcName, void* newFn) {
    HMODULE h = GetModuleHandleA(nullptr);
    return hook(h, dllName, funcName, newFn);
}

} // namespace iat
