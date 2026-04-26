#include "pe_utils.hpp"

namespace pe {

PIMAGE_NT_HEADERS headers(HMODULE mod) {
    if (!mod) mod = GetModuleHandleA(nullptr);
    auto base = (BYTE*)mod;
    auto dos  = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    auto nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    return nt;
}

bool setCharacteristic(HMODULE mod, WORD flag, bool on) {
    auto nt = headers(mod);
    if (!nt) return false;

    auto* fh = &nt->FileHeader;
    DWORD oldProt = 0;
    if (!VirtualProtect(fh, sizeof(*fh), PAGE_READWRITE, &oldProt)) return false;

    if (on) fh->Characteristics |= flag;
    else    fh->Characteristics &= ~flag;

    DWORD tmp;
    VirtualProtect(fh, sizeof(*fh), oldProt, &tmp);
    return true;
}

bool codeRange(HMODULE mod, BYTE** start, SIZE_T* size) {
    auto nt = headers(mod);
    if (!nt) return false;

    auto section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, section++) {
        if (section->Characteristics & IMAGE_SCN_CNT_CODE) {
            *start = (BYTE*)mod + section->VirtualAddress;
            *size  = section->Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

} // namespace pe
