#pragma once

#include <cstddef>
#include <cstdint>

#include "gc_malloc/ThreadHeap/MemSubPoolList.hpp"

class MemSubPool;


class SizeClassPoolManager {
public:
    static constexpr std::size_t kTargetEmptyWatermark = 2; // 目标/中间水位
    static constexpr std::size_t kHighEmptyWatermark   = 4; // 最高水位

    using RefillCallback = MemSubPool* (*)(void* ctx) noexcept;             // 供补充空闲子池
    using ReturnCallback = void (*)(void* ctx, MemSubPool* pool) noexcept;   // 供交还空闲子池

public:
    explicit SizeClassPoolManager(std::size_t block_size) noexcept;
    ~SizeClassPoolManager();

    SizeClassPoolManager(const SizeClassPoolManager&)            = delete;
    SizeClassPoolManager& operator=(const SizeClassPoolManager&) = delete;
    SizeClassPoolManager(SizeClassPoolManager&&)                 = delete;
    SizeClassPoolManager& operator=(SizeClassPoolManager&&)      = delete;

    void SetRefillCallback(RefillCallback cb, void* ctx) noexcept;
    void SetReturnCallback(ReturnCallback cb, void* ctx) noexcept;

    void* AllocateBlock() noexcept;
    bool  ReleaseBlock(void* ptr) noexcept;

    std::size_t GetBlockSize()        const noexcept;
    std::size_t GetPoolCountEmpty()   const noexcept;
    std::size_t GetPoolCountPartial() const noexcept;
    std::size_t GetPoolCountFull()    const noexcept;

    bool OwnsPointer(const void* ptr) const noexcept;

private:
    static inline bool PoolIsEmpty_(const MemSubPool* p) noexcept;
    static inline bool PoolIsFull_ (const MemSubPool* p) noexcept;

    static MemSubPool* PtrToOwnerPool_(const void* block_ptr) noexcept;

    void RefillEmptyPools_() noexcept; // empty 为空则补齐到 kTargetEmptyWatermark
    void TrimEmptyPools_() noexcept;   // empty 超过最高水位则回落到目标水位

    MemSubPool* AcquireUsablePool_() noexcept;

private:
    const std::size_t block_size_;

    MemSubPoolList empty_;
    MemSubPoolList partial_;
    MemSubPoolList full_;

    RefillCallback refill_cb_  = nullptr;
    ReturnCallback return_cb_  = nullptr;
    void*          refill_ctx_ = nullptr;
    void*          return_ctx_ = nullptr;
};
