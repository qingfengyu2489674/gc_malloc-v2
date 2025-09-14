#pragma once

#include <cstddef>

class ChunkAllocatorFromKernel;
class FreeChunkCache;

class CentralHeap {
public:
    static CentralHeap& GetInstance();

    void* acquire_chunk(size_t size);
    void release_chunk(void* chunk, size_t size);

    CentralHeap(const CentralHeap&) = delete;
    CentralHeap& operator=(const CentralHeap&) = delete;
    CentralHeap(CentralHeap&&) = delete;
    CentralHeap& operator=(CentralHeap&&) = delete;

private:
    CentralHeap();
    virtual ~CentralHeap(); 

    bool refill_cache(); 

    ChunkAllocatorFromKernel* pChunkAllocatorFromKernel = nullptr;
    FreeChunkCache* pFreeChunkCache = nullptr;

    static constexpr size_t kChunkSize = 2 * 1024 *1024;
    static constexpr size_t kMaxWatermarkInChunks = 16;
    static constexpr size_t kTargetWatermarkInChunks = 8;
};

