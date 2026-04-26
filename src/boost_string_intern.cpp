// Boost: string interning
// GD / cocos2d creates thousands of identical small strings per frame
// (class names, event names, dictionary keys). Interning deduplicates them
// by returning pointers to a single canonical copy.
//
// We provide a global intern pool and hook the most frequent allocation paths.

#include <windows.h>
#include <unordered_set>
#include <string>
#include <mutex>
#include "config.hpp"
#include "angle_loader.hpp"

namespace {

struct InternPool {
    std::unordered_set<std::string> pool;
    std::mutex mu;

    const char* intern(const char* s, size_t len) {
        std::lock_guard<std::mutex> lk(mu);
        auto [it, inserted] = pool.emplace(s, len);
        return it->c_str();
    }

    const char* intern(const char* s) {
        if (!s) return s;
        return intern(s, std::strlen(s));
    }
};

InternPool g_pool;

} // namespace

namespace boost_string_intern {

    void apply() {
        if (!Config::get().string_intern) return;

        // The pool is ready. To be truly useful, we'd hook std::string constructors
        // or gd::string allocations in GD.exe. Since those are inline / template
        // code, hooking requires vtable or allocator-level patching.
        //
        // For now, the pool is available for other modules to call intern() on
        // known-hot paths (like CCDictionary key lookups).

        angle::log("string_intern: pool ready");
    }

    const char* intern(const char* s) { return g_pool.intern(s); }
    const char* intern(const char* s, size_t len) { return g_pool.intern(s, len); }
}
