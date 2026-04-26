#include "dxt1_encoder.hpp"
#include <algorithm>
#include <cstring>

namespace dxt1 {

static uint16_t rgb888to565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void findMinMax(const uint8_t block[64], uint8_t minC[3], uint8_t maxC[3]) {
    minC[0] = minC[1] = minC[2] = 255;
    maxC[0] = maxC[1] = maxC[2] = 0;
    for (int i = 0; i < 16; i++) {
        int off = i * 4;
        for (int c = 0; c < 3; c++) {
            if (block[off + c] < minC[c]) minC[c] = block[off + c];
            if (block[off + c] > maxC[c]) maxC[c] = block[off + c];
        }
    }
}

static uint32_t dist(const uint8_t* a, const uint8_t* b) {
    int dr = (int)a[0] - b[0];
    int dg = (int)a[1] - b[1];
    int db = (int)a[2] - b[2];
    return (uint32_t)(dr * dr + dg * dg + db * db);
}

void compressBlock(const uint8_t block[64], uint8_t out[8]) {
    uint8_t minC[3], maxC[3];
    findMinMax(block, minC, maxC);

    uint16_t c0 = rgb888to565(maxC[0], maxC[1], maxC[2]);
    uint16_t c1 = rgb888to565(minC[0], minC[1], minC[2]);

    if (c0 < c1) { std::swap(c0, c1); std::swap(minC[0], maxC[0]); std::swap(minC[1], maxC[1]); std::swap(minC[2], maxC[2]); }
    if (c0 == c1) { c0 = (c0 < 0xFFFF) ? c0 + 1 : c0; }

    // build palette
    uint8_t palette[4][3];
    std::memcpy(palette[0], maxC, 3);
    std::memcpy(palette[1], minC, 3);
    for (int c = 0; c < 3; c++) {
        palette[2][c] = (uint8_t)((2 * maxC[c] + minC[c] + 1) / 3);
        palette[3][c] = (uint8_t)((maxC[c] + 2 * minC[c] + 1) / 3);
    }

    uint32_t indices = 0;
    for (int i = 15; i >= 0; i--) {
        const uint8_t* px = block + i * 4;
        uint32_t bestDist = 0xFFFFFFFF;
        uint32_t bestIdx = 0;
        for (uint32_t j = 0; j < 4; j++) {
            uint32_t d = dist(px, palette[j]);
            if (d < bestDist) { bestDist = d; bestIdx = j; }
        }
        indices = (indices << 2) | bestIdx;
    }

    out[0] = (uint8_t)(c0);
    out[1] = (uint8_t)(c0 >> 8);
    out[2] = (uint8_t)(c1);
    out[3] = (uint8_t)(c1 >> 8);
    out[4] = (uint8_t)(indices);
    out[5] = (uint8_t)(indices >> 8);
    out[6] = (uint8_t)(indices >> 16);
    out[7] = (uint8_t)(indices >> 24);
}

size_t compress(const uint8_t* rgba, int width, int height, uint8_t* dst) {
    if (width < 4 || height < 4) return 0;
    if ((width & 3) || (height & 3)) return 0;
    if (!rgba || !dst) return 0;

    int bw = width / 4;
    int bh = height / 4;
    size_t outOff = 0;

    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            uint8_t block[64];
            for (int row = 0; row < 4; row++) {
                int srcY = by * 4 + row;
                int srcX = bx * 4;
                std::memcpy(block + row * 16, rgba + (srcY * width + srcX) * 4, 16);
            }
            compressBlock(block, dst + outOff);
            outOff += 8;
        }
    }
    return outOff;
}

} // namespace dxt1
