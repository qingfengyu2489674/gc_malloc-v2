#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

#include "gc_malloc/ThreadHeap/SizeClassPoolManager.hpp"
#include "gc_malloc/ThreadHeap/ManagedList.hpp"
#include "gc_malloc/ThreadHeap/BlockHeader.hpp"
#include "gc_malloc/ThreadHeap/SizeClassConfig.hpp"

class MemSubPool;

/**
 * ThreadHeap
 * ------------------------------------------------------------------
 * 线程本地分配器（TLS 内部实例）。
 */
class ThreadHeap {
public:
    // --------------------- 对外公共接口 ---------------------
    static void*        Allocate(std::size_t nbytes) noexcept;
    static void         Deallocate(void* ptr) noexcept;
    static std::size_t  GarbageCollect(std::size_t max_scan = SIZE_MAX) noexcept;

    ThreadHeap(const ThreadHeap&)            = delete;
    ThreadHeap& operator=(const ThreadHeap&) = delete;
    ThreadHeap(ThreadHeap&&)                 = delete;
    ThreadHeap& operator=(ThreadHeap&&)      = delete;

private:
    static ThreadHeap& Local_() noexcept;

    ThreadHeap() noexcept;
    ~ThreadHeap();

    static std::size_t SizeToClass_(std::size_t nbytes) noexcept;

    // ---- 与 SizeClassPoolManager 的回调桥 ----
    static MemSubPool* RefillFromMgrCb_(void* ctx) noexcept;                 // 获取新子池
    static void        ReturnToCentralCb_(void* ctx, MemSubPool* p) noexcept; // 归还空子池

    // ---- 小工具 ----
    void        AttachUsed_(BlockHeader* blk) noexcept;
    std::size_t ReclaimOnce_(std::size_t max_scan) noexcept;

private:
    // ★ 编译期常量（来自 SizeClassConfig.hpp，必须是 constexpr）
    static constexpr std::size_t kClassCount = SizeClassConfig::kClassCount;

    // ★ 原始对齐存储，避免默认构造；绝不额外分配
    using ManagerStorage =
        std::aligned_storage_t<sizeof(SizeClassPoolManager), alignof(SizeClassPoolManager)>;
    ManagerStorage managers_storage_[kClassCount];

    // ★ 便捷访问封装
    static SizeClassPoolManager& at(ManagerStorage& s) noexcept {
        return *reinterpret_cast<SizeClassPoolManager*>(&s);
    }
    static const SizeClassPoolManager& at(const ManagerStorage& s) noexcept {
        return *reinterpret_cast<const SizeClassPoolManager*>(&s);
    }

    ManagedList managed_list_;
};
