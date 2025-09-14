#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <new>

constexpr size_t CACHE_LINE_SIZE = 64;

class alignas(CACHE_LINE_SIZE) MemSubPool {
public:
    static constexpr size_t kPoolTotalSize = 2 * 1024 * 1024; // 2MB
    static constexpr size_t kPoolAlignment = kPoolTotalSize;
    static constexpr size_t kMinBlockSize = 32; 
    static constexpr size_t kBitMapLength = (kPoolTotalSize / kMinBlockSize + 7) / 8;
    static constexpr uint32_t kPoolMagic = 0xDEADBEEF;

private:
    const uint32_t magic = kPoolMagic;
    char pad1[CACHE_LINE_SIZE - sizeof(uint32_t)];
    std::mutex lock;

    const size_t block_size;
    size_t total_block_count;
    size_t used_block_count;

    unsigned char bitmap_buffer_[kBitMapLength];
    Bitmap bitmap_;
}