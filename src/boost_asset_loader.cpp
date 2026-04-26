// Boost: async asset loader
// Provides a thread pool that pre-reads files into the OS file cache.
// This doesn't replace cocos2d's texture loading (which must happen on the GL
// thread), but it warms the file cache so that when cocos2d does load a texture,
// the data is already in RAM and the read completes instantly.

#include <windows.h>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <atomic>
#include "config.hpp"
#include "angle_loader.hpp"

namespace {

struct Pool {
    std::vector<std::thread> workers;
    std::queue<std::string>  tasks;
    std::mutex               mu;
    std::condition_variable  cv;
    std::atomic<bool>        stop{false};

    void start(int n) {
        for (int i = 0; i < n; i++) {
            workers.emplace_back([this] { work(); });
        }
    }

    void enqueue(const std::string& path) {
        { std::lock_guard<std::mutex> lk(mu); tasks.push(path); }
        cv.notify_one();
    }

    void shutdown() {
        stop.store(true);
        cv.notify_all();
        for (auto& w : workers) if (w.joinable()) w.join();
    }

    void work() {
        while (true) {
            std::string path;
            {
                std::unique_lock<std::mutex> lk(mu);
                cv.wait(lk, [&] { return stop.load() || !tasks.empty(); });
                if (stop.load() && tasks.empty()) return;
                path = tasks.front();
                tasks.pop();
            }
            // just read the file to warm the OS cache
            HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING,
                                   FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                char buf[64 * 1024];
                DWORD read = 0;
                while (ReadFile(h, buf, sizeof(buf), &read, nullptr) && read > 0) {}
                CloseHandle(h);
            }
        }
    }
};

Pool* g_pool = nullptr;

} // namespace

namespace boost_asset_loader {

    void apply() {
        auto& cfg = Config::get();
        if (!cfg.async_asset_loader) return;

        int threads = cfg.async_loader_threads;
        if (threads < 1) threads = 2;
        if (threads > 16) threads = 16;

        g_pool = new Pool();
        g_pool->start(threads);
        angle::log("asset_loader: pool started with %d threads", threads);

        // pre-warm: queue all .png files in Resources/
        WIN32_FIND_DATAA fd;
        HANDLE fh = FindFirstFileA("Resources\\*.png", &fd);
        if (fh != INVALID_HANDLE_VALUE) {
            int count = 0;
            do {
                g_pool->enqueue(std::string("Resources\\") + fd.cFileName);
                count++;
            } while (FindNextFileA(fh, &fd));
            FindClose(fh);
            angle::log("asset_loader: queued %d PNG files for pre-warming", count);
        }
    }

    void shutdown() {
        if (g_pool) { g_pool->shutdown(); delete g_pool; g_pool = nullptr; }
    }
}
