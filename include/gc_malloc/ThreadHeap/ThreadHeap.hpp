#pragma once

#include <cstddef>
#include <cstdint>

#include "gc_malloc/ThreadHeap/SizeClassPoolManager.hpp"
#include "gc_malloc/ThreadHeap/ManagedList.hpp"
#include "gc_malloc/ThreadHeap/BlockHeader.hpp"
#include "gc_malloc/Config/SizeClassConfig.hpp"

class MemSubPool;

/**
 * ThreadHeap
 * ------------------------------------------------------------------
 * 线程本地分配器（TLS 内部实例）。对外仅暴露三种操作：
 *   1) Allocate(nbytes)        —— 分配
 *   2) Deallocate(ptr)         —— 释放（可跨线程调用；仅改块头标志为 Free）
 *   3) GarbageCollect()        —— 垃圾回收（仅在当前线程，对本线程托管链表做摘除回收）
 *
 * 其他细节（托管链表遍历、size-class 管理器回调等）均为内部实现。
 */
class ThreadHeap {
public:
    // --------------------- 对外公共接口（仅保留必要项） ---------------------

    // 分配：基于当前线程的 TLS 实例完成小对象分配（大对象走 CentralHeap）
    static void* Allocate(std::size_t nbytes) noexcept;

    // 释放：跨线程安全；仅把块头标记为 Free，真正回收由 GarbageCollect 执行
    static void  Deallocate(void* ptr) noexcept;

    // 垃圾回收：在当前线程上，从托管链表扫描并回收已 Free 的块
    // 返回本次回收的块数
    static std::size_t GarbageCollect(std::size_t max_scan = SIZE_MAX) noexcept;

    // 禁止拷贝/移动
    ThreadHeap(const ThreadHeap&)            = delete;
    ThreadHeap& operator=(const ThreadHeap&) = delete;
    ThreadHeap(ThreadHeap&&)                 = delete;
    ThreadHeap& operator=(ThreadHeap&&)      = delete;

private:
    // --------------------- 内部实现细节（不对外暴露） ---------------------

    // TLS 入口：获取当前线程实例（必要时懒初始化）
    static ThreadHeap& Local_() noexcept;

    // 构造/析构仅供 TLS 管理调用
    ThreadHeap() noexcept;
    ~ThreadHeap();

    // 将请求字节数映射为 size-class 下标
    static std::size_t SizeToClass_(std::size_t nbytes) noexcept;

    // ---- 与 SizeClassPoolManager 的回调桥 ----
    static MemSubPool* RefillFromMgrCb_(void* ctx) noexcept;                 // 获取新子池
    static void        ReturnToCentralCb_(void* ctx, MemSubPool* p) noexcept; // 归还空子池

    // ---- 小工具 ----
    // 把“已分配出去的块”挂到托管链表（进入托管即标记为 Used）
    void AttachUsed_(BlockHeader* blk) noexcept;

    // 执行一次扫描：若发现 Free 块，摘链并交由对应 manager 回收；返回回收个数增量
    std::size_t ReclaimOnce_(std::size_t max_scan) noexcept;

private:
    static constexpr std::size_t kClassCount = SizeClassConfig::ClassCount();
    std::size_t          class_count_ = 0;
    SizeClassPoolManager managers_[kClassCount];

    // 托管链表（仅当前线程访问）
    ManagedList managed_list_;

};
