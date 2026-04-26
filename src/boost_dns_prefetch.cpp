// Boost: DNS Prefetch
// Pre-resolves GD server hostnames at startup so subsequent connections
// don't stall on DNS lookup (which can take 50-200ms on slow DNS servers).

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include "config.hpp"
#include "angle_loader.hpp"

#pragma comment(lib, "ws2_32.lib")

namespace {

static const char* g_hosts[] = {
    "www.boomlings.com",
    "www.robtopgames.com",
    "geometrydash.eu",
    "gdbrowser.com",
    nullptr
};

void prefetchThread() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;

    int resolved = 0;
    for (int i = 0; g_hosts[i]; i++) {
        struct addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* result = nullptr;

        if (getaddrinfo(g_hosts[i], "80", &hints, &result) == 0) {
            resolved++;
            freeaddrinfo(result);
        }
    }

    angle::log("dns_prefetch: resolved %d hosts", resolved);
}

} // namespace

namespace boost_dns_prefetch {
    void apply() {
        if (!Config::get().dns_prefetch) return;

        // Do DNS resolution on a background thread to not block startup
        std::thread(prefetchThread).detach();
        angle::log("dns_prefetch: started background resolution");
    }
}
