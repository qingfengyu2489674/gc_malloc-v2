#include <new>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <cassert>

#include "gc_malloc/CentralHeap/CentralHeap.hpp"
#include "gc_malloc/ThreadHeap/MemSubPool.hpp"
#include "gc_malloc/ThreadHeap/ThreadHeap.hpp"

// --------------- 对外公共接口 ---------------

void* ThreadHeap::Allocate(std::size_t nbytes) noexcept {
    ThreadHeap& th = Local_();

    // 大对象：直接走 CentralHeap
    if (nbytes > SizeClassConfig::kMaxSmallAlloc) {
        // 注意：CentralHeap::kChunkSize 是私有的，改用 SizeClassConfig::kChunkSizeBytes
        void* raw = CentralHeap::GetInstance().AcquireChunk(SizeClassConfig::kChunkSizeBytes);
        return raw;
    }

    // 小对象：映射到 size-class，向该 class 的池子分配
    const std::size_t class_idx = SizeToClass_(nbytes);
    void* block_ptr = at(th.managers_storage_[class_idx]).AllocateBlock();  // ★ 修正：使用 at(...)
    if (!block_ptr) return nullptr;

    // 将块挂入托管链表（进入托管即标记为 Used）
    auto* hdr = static_cast<BlockHeader*>(block_ptr);
    th.AttachUsed_(hdr);
    return block_ptr;
}

void ThreadHeap::Deallocate(void* ptr) noexcept {
    if (!ptr) return;

    // 跨线程释放：仅写块头状态为 Free，真正回收由拥有该块的线程在 GC 中处理
    auto* hdr = static_cast<BlockHeader*>(ptr);
    hdr->store_free();
}

std::size_t ThreadHeap::GarbageCollect(std::size_t max_reclaim) noexcept {
    ThreadHeap& th = Local_();
    return th.ReclaimOnce_(max_reclaim);
}

// --------------- 内部实现（TLS / 构造 / 回调桥等） ---------------

ThreadHeap& ThreadHeap::Local_() noexcept {
    static thread_local ThreadHeap tls_instance;
    return tls_instance;
}

ThreadHeap::ThreadHeap() noexcept {
    for (std::size_t i = 0; i < kClassCount; ++i) {
        const std::size_t bs = SizeClassConfig::ClassToSize(i);
        void* slot = static_cast<void*>(&managers_storage_[i]);
        new (slot) SizeClassPoolManager(bs);  // ★ placement-new

        // 如果需要设置回调：ctx 必须能在回调里恢复出 SizeClassPoolManager&
        // 这里将 ctx 传递为 ManagerStorage*，回调中用 at(*ptr) 还原引用
        at(managers_storage_[i]).SetRefillCallback(&ThreadHeap::RefillFromMgrCb_, /*ctx=*/&managers_storage_[i]);
        at(managers_storage_[i]).SetReturnCallback(&ThreadHeap::ReturnToCentralCb_, /*ctx=*/&managers_storage_[i]);
    }
}

ThreadHeap::~ThreadHeap() {
    for (std::size_t i = 0; i < kClassCount; ++i) {                  // ★ 修正：kClassCount
        at(managers_storage_[i]).~SizeClassPoolManager();            // ★ 修正：显式析构，走 at(...)
    }
}

std::size_t ThreadHeap::SizeToClass_(std::size_t nbytes) noexcept {
    return SizeClassConfig::SizeToClass(nbytes);
}

// ---- 与 SizeClassPoolManager 的回调桥 ----

MemSubPool* ThreadHeap::RefillFromMgrCb_(void* ctx) noexcept {
    // ctx 是传进来的 ManagerStorage*，需要还原出 SizeClassPoolManager&
    auto* storage_ptr = static_cast<ManagerStorage*>(ctx);
    SizeClassPoolManager& mgr = at(*storage_ptr);
    const std::size_t bs = mgr.GetBlockSize();

    // 同上，不能用 CentralHeap::kChunkSize（它是私有的）
    void* raw = CentralHeap::GetInstance().AcquireChunk(SizeClassConfig::kChunkSizeBytes);
    if (!raw) return nullptr;

    return new (raw) MemSubPool(bs);
}

void ThreadHeap::ReturnToCentralCb_(void* ctx, MemSubPool* p) noexcept {
    (void)ctx; // 当前未使用
    if (!p) return;

    // 析构子池，然后把整块 chunk 归还给 CentralHeap
    p->~MemSubPool();
    CentralHeap::GetInstance().ReleaseChunk(static_cast<void*>(p), SizeClassConfig::kChunkSizeBytes);
}

// ---- 小工具：托管链表封装 ----

void ThreadHeap::AttachUsed_(BlockHeader* blk) noexcept {
    if (!blk) return;
    managed_list_.attach_used(blk);
}

std::size_t ThreadHeap::ReclaimOnce_(std::size_t max_reclaim) noexcept {
    std::size_t reclaimed = 0;
    std::size_t scanned   = 0;

    managed_list_.reset_cursor();

    while (scanned < max_reclaim) {
        BlockHeader* freed = managed_list_.reclaim_next();
        if (!freed) break; // 本轮没有可回收的块了

        ++scanned;

        void* user_ptr = static_cast<void*>(freed);

        // 线性探测归还（简单但正确；后续可优化成指针→子池的快速定位）
        bool released = false;
        for (std::size_t i = 0; i < kClassCount; ++i) {                         // ★ 修正：kClassCount
            if (at(managers_storage_[i]).ReleaseBlock(user_ptr)) {              // ★ 修正：使用 at(...)
                released = true;
                break;
            }
        }
        assert(released && "ReclaimOnce_: block did not belong to any SizeClassPoolManager!");

        if (released) ++reclaimed;
    }

    return reclaimed;
}
