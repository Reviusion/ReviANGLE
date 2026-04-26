#pragma once
#include <cstddef>
#include <cstdint>

// Simple ring buffer for VBO sub-allocation.
// Not thread-safe (only main thread touches GL).

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) : m_cap(capacity) {}

    // Tries to allocate `size` bytes. Returns offset or SIZE_MAX on overflow.
    size_t alloc(size_t size) {
        if (size > m_cap) return SIZE_MAX;

        size_t end = m_offset + size;
        if (end > m_cap) {
            // wrap around
            m_offset = 0;
            end = size;
            m_generation++;
        }
        size_t off = m_offset;
        m_offset = end;
        return off;
    }

    void reset() { m_offset = 0; m_generation++; }
    size_t capacity() const { return m_cap; }
    size_t offset()   const { return m_offset; }
    uint32_t gen()    const { return m_generation; }

private:
    size_t   m_cap = 0;
    size_t   m_offset = 0;
    uint32_t m_generation = 0;
};
