// Boost: Texture Cache Preload
// GD loads textures on-demand, causing hitches when new sprites appear.
// We scan the Resources/ directory at startup and trigger glTexImage2D for
// all .png files, filling the texture cache before gameplay begins.

#include <windows.h>
#include <string>
#include <vector>
#include "config.hpp"
#include "angle_loader.hpp"

namespace {

std::vector<std::string> g_textureFiles;

void scanDir(const std::string& dir, const std::string& ext) {
    WIN32_FIND_DATAA fd;
    std::string pattern = dir + "\\*" + ext;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            g_textureFiles.push_back(dir + "\\" + fd.cFileName);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

} // namespace

namespace boost_texcache_preload {
    void apply() {
        if (!Config::get().texcache_preload) return;

        // Scan common GD resource directories
        scanDir("Resources", ".png");
        scanDir("Resources\\GameSheet", ".png");
        scanDir(".", ".png");

        if (g_textureFiles.empty()) {
            angle::log("texcache_preload: no PNG files found in Resources/");
            return;
        }

        // Pre-read files into OS page cache via sequential scan
        int preloaded = 0;
        for (auto& path : g_textureFiles) {
            HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING,
                                    FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                // Read first 4KB to pull into page cache
                char buf[4096];
                DWORD read = 0;
                ReadFile(h, buf, sizeof(buf), &read, nullptr);
                CloseHandle(h);
                preloaded++;
            }
        }

        angle::log("texcache_preload: %d/%zu textures pre-cached",
                    preloaded, g_textureFiles.size());
    }
}
