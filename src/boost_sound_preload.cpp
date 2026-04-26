// Boost: Sound Preload
// Pre-reads all .mp3 and .ogg files from Resources/ into the OS page cache
// at startup. When FMOD opens them later, they'll be served from RAM.

#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include "config.hpp"
#include "angle_loader.hpp"

namespace {

void scanAndPreload(const std::string& dir, const std::string& ext) {
    WIN32_FIND_DATAA fd;
    std::string pattern = dir + "\\*" + ext;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    int count = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string path = dir + "\\" + fd.cFileName;

        HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING,
                                FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            // Read entire file to pull into page cache
            char buf[8192];
            DWORD read = 0;
            while (ReadFile(f, buf, sizeof(buf), &read, nullptr) && read > 0) {}
            CloseHandle(f);
            count++;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);

    if (count > 0) {
        angle::log("sound_preload: %d %s files cached from %s",
                    count, ext.c_str(), dir.c_str());
    }
}

void preloadThread() {
    scanAndPreload("Resources", ".mp3");
    scanAndPreload("Resources", ".ogg");
    scanAndPreload(".", ".mp3");
    scanAndPreload(".", ".ogg");
}

} // namespace

namespace boost_sound_preload {
    void apply() {
        if (!Config::get().sound_preload) return;

        std::thread(preloadThread).detach();
        angle::log("sound_preload: background preload started");
    }
}
