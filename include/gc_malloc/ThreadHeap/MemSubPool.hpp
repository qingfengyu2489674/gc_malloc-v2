#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <new>

#include "gc_malloc/ThreadHeap/Bitmap.hpp"

constexpr size_t CACHE_LINE_SIZE = 64;

class alignas(CACHE_LINE_SIZE) MemSubPool {
public:
    static constexpr size_t kPoolTotalSize = 2 * 1024 * 1024; // 2MB
    static constexpr size_t kPoolAlignment = kPoolTotalSize;
    static constexpr size_t kMinBlockSize = 32; 
    static constexpr size_t kBitMapLength = (kPoolTotalSize / kMinBlockSize + 7) / 8;
    static constexpr uint32_t kPoolMagic = 0xDEADBEEF;

public:
    explicit MemSubPool(size_t block_size);
    ~MemSubPool();

    void* Allocate();
    void Release(void* block_ptr);
    
    // 提供一些接口给 CentralHeap 使用
    bool IsFull() const;
    bool IsEmpty() const;
    size_t GetBlockSize() const;

private:
    static size_t CalculateDataOffset();
    static size_t CalculateTotalBlockCount(size_t block_size, size_t data_offset);

    const uint32_t magic_;
    char pad1_[CACHE_LINE_SIZE - sizeof(uint32_t)];
    std::mutex lock_;

    const size_t block_size_;
    const size_t data_offset_;
    const size_t total_block_count_;
    std::atomic<size_t> used_block_count_;
    size_t next_free_block_hint_;

    unsigned char bitmap_buffer_[kBitMapLength];
    Bitmap bitmap_;

    MemSubPool(const MemSubPool&) = delete;
    MemSubPool& operator=(const MemSubPool&) = delete;
    MemSubPool(MemSubPool&&) = delete;
    MemSubPool& operator=(MemSubPool&&) = delete;
};