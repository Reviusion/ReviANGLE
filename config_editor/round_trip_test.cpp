// Tiny CLI test: load angle_config.ini, flip frame_pacing's value, save to a
// temp path, and report a one-line diff so we can verify that EVERY non-
// edited line is preserved byte-for-byte.
//
// Built only when DESIGN_TEST_ROUND_TRIP is defined. Not part of the normal
// CMake build to keep the editor exe lean.

#include "ini_parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

static std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> out;
    std::ifstream f(path, std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(line);
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: round_trip_test <path-to-ini>\n";
        return 2;
    }
    const std::string in  = argv[1];
    const std::string out = in + ".rtt";

    Ini ini;
    if (!ini.load(in)) {
        std::cerr << "failed to load " << in << "\n";
        return 1;
    }

    // Flip frame_pacing: read current value, write the opposite.
    std::string cur = ini.get("BoostLatency", "frame_pacing");
    std::string flipped = (cur == "true" ? "false" : "true");
    ini.set("BoostLatency", "frame_pacing", flipped);

    if (!ini.save(out)) {
        std::cerr << "failed to save " << out << "\n";
        return 1;
    }

    auto a = readLines(in);
    auto b = readLines(out);

    int diffs = 0;
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            ++diffs;
            std::cout << "Line " << (i+1) << " differs:\n";
            std::cout << "  - " << a[i] << "\n";
            std::cout << "  + " << b[i] << "\n";
        }
    }
    if (a.size() != b.size()) {
        std::cout << "Size differs: in=" << a.size() << " out=" << b.size() << "\n";
        ++diffs;
    }

    std::cout << "TOTAL DIFFERING LINES: " << diffs << " (expected: 1)\n";
    std::cout << "ORIGINAL: " << a.size() << " lines\n";
    std::cout << "ROUND-TRIP: " << b.size() << " lines\n";

    return diffs == 1 ? 0 : 1;
}
