// tests/SizeClassPoolManager_test.cpp

#include "gtest/gtest.h"

#include "gc_malloc/ThreadHeap/MemSubPool.hpp"
#include "gc_malloc/ThreadHeap/MemSubPoolList.hpp"
#include "gc_malloc/ThreadHeap/SizeClassPoolManager.hpp"

#include <new>        // std::align_val_t
#include <vector>
#include <cstdint>
#include <algorithm>

// ============== 测试上下文与回调实现 ==============
// 测试桩：用对齐的 ::operator new/delete 获取/释放 2MB 原始内存，
// 在其起始地址 placement-new / 显式析构 MemSubPool。
// 仅用于测试，不改变生产代码“禁止系统 new”的约束。

struct TestPoolIOCtx {
    std::size_t         block_size = 64;
    std::vector<void*>  allocated_chunks;   // 记录已分配的 2MB chunk 起始指针
    std::size_t         refill_calls = 0;
    std::size_t         return_calls = 0;

    static void* PoolToChunk(void* pool_base) { return pool_base; }

    // 在 manager 销毁后，可调用该函数释放仍未通过回调归还的 chunk
    void ForceCleanupAll() {
        for (void* chunk : allocated_chunks) {
            if (!chunk) continue;
            auto* pool = reinterpret_cast<MemSubPool*>(chunk);
            pool->~MemSubPool();
            ::operator delete(chunk, std::align_val_t(MemSubPool::kPoolAlignment));
        }
        allocated_chunks.clear();
    }
};

static MemSubPool* TestRefillCallback(void* ctx) noexcept {
    auto* c = static_cast<TestPoolIOCtx*>(ctx);
    ++c->refill_calls;

    void* mem = nullptr;
    try {
        mem = ::operator new(MemSubPool::kPoolTotalSize,
                             std::align_val_t(MemSubPool::kPoolAlignment));
    } catch (...) {
        return nullptr;
    }
    if (!mem) return nullptr;

    auto* pool = new (mem) MemSubPool(c->block_size);
    c->allocated_chunks.push_back(mem);

    EXPECT_TRUE(pool->IsEmpty());
    EXPECT_EQ(pool->GetBlockSize(), c->block_size);
    EXPECT_EQ(pool->list_prev, nullptr);
    EXPECT_EQ(pool->list_next, nullptr);
    return pool;
}

static void TestReturnCallback(void* ctx, MemSubPool* pool) noexcept {
    auto* c = static_cast<TestPoolIOCtx*>(ctx);
    ++c->return_calls;
    ASSERT_NE(pool, nullptr);

    void* chunk = TestPoolIOCtx::PoolToChunk(pool);
    pool->~MemSubPool();
    ::operator delete(chunk, std::align_val_t(MemSubPool::kPoolAlignment));

    auto it = std::find(c->allocated_chunks.begin(), c->allocated_chunks.end(), chunk);
    if (it != c->allocated_chunks.end()) c->allocated_chunks.erase(it);
}

// 小工具：查看三条链规模（仅用于断言）
static std::size_t EmptyCount(SizeClassPoolManager& mgr)   { return mgr.getPoolCountEmpty();   }
static std::size_t PartialCount(SizeClassPoolManager& mgr) { return mgr.getPoolCountPartial(); }
static std::size_t FullCount(SizeClassPoolManager& mgr)    { return mgr.getPoolCountFull();    }

// ============== 测试 1：按需补水 + 基本分配/释放 ==============
// 期望：第一次分配时触发补水（empty 从 0 补到 target），分配后 pool 入 partial；
// 释放后该 pool 回到 empty。
TEST(SizeClassPoolManager, RefillOnDemand_Allocate_Release) {
    TestPoolIOCtx ctx;
    ctx.block_size = 64;

    {
        SizeClassPoolManager mgr{ctx.block_size};
        mgr.setRefillCallback(&TestRefillCallback, &ctx);
        mgr.setReturnCallback(&TestReturnCallback, &ctx);

        EXPECT_EQ(EmptyCount(mgr),   0u);
        EXPECT_EQ(PartialCount(mgr), 0u);
        EXPECT_EQ(FullCount(mgr),    0u);

        // 第一次分配：应触发按需补水
        void* p = mgr.allocateBlock();
        ASSERT_NE(p, nullptr);
        EXPECT_GE(ctx.refill_calls, 1u);

        // 分配后：至少一个 pool 入 partial
        EXPECT_EQ(PartialCount(mgr), 1u);
        // empty 可能为 target-1（因为先补到 target，再从 empty 取了一个 pool 用于分配）
        EXPECT_LE(EmptyCount(mgr), SizeClassPoolManager::kTargetEmptyWatermark);

        // 释放：回到 empty
        EXPECT_TRUE(mgr.releaseBlock(p));
        EXPECT_EQ(PartialCount(mgr), 0u);
        EXPECT_GE(EmptyCount(mgr), 1u);
    }

    // 此时 manager 已析构；测试桩释放仍由我们兜底清理
    ctx.ForceCleanupAll();
}

// ============== 测试 2：多次分配同一子池 + 释放回落 ==============
// 期望：多次 AllocateBlock（不至于把 pool 撑满），partial 规模保持合理；
// 依次释放后回到 empty。
TEST(SizeClassPoolManager, MultipleAllocations_SamePool) {
    TestPoolIOCtx ctx;
    ctx.block_size = 64;

    {
        SizeClassPoolManager mgr{ctx.block_size};
        mgr.setRefillCallback(&TestRefillCallback, &ctx);
        mgr.setReturnCallback(&TestReturnCallback, &ctx);

        // 第一次分配触发补水
        void* a = mgr.allocateBlock();
        ASSERT_NE(a, nullptr);
        EXPECT_EQ(PartialCount(mgr), 1u);

        // 再分配几个块（很大概率仍来自同一 pool）
        void* b = mgr.allocateBlock();
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(PartialCount(mgr), 1u);

        void* c = mgr.allocateBlock();
        ASSERT_NE(c, nullptr);
        EXPECT_EQ(PartialCount(mgr), 1u);

        // 释放三个块：partial 仍然是 1（该 pool 部分使用）
        EXPECT_TRUE(mgr.releaseBlock(b));
        EXPECT_EQ(PartialCount(mgr), 1u);

        EXPECT_TRUE(mgr.releaseBlock(c));
        EXPECT_EQ(PartialCount(mgr), 1u);

        // 释放最后一个：该 pool 回到 empty
        EXPECT_TRUE(mgr.releaseBlock(a));
        EXPECT_EQ(PartialCount(mgr), 0u);
        EXPECT_GE(EmptyCount(mgr), 1u);
    }

    ctx.ForceCleanupAll();
}

// ============== 测试 3：OwnsPointer 宽松判定（不同块大小时应为 false） ==============
TEST(SizeClassPoolManager, OwnsPointer_BlockSizeMismatch) {
    TestPoolIOCtx ctxA; ctxA.block_size = 64;
    TestPoolIOCtx ctxB; ctxB.block_size = 128;

    void* p = nullptr;

    {
        SizeClassPoolManager mgrA{ctxA.block_size};
        SizeClassPoolManager mgrB{ctxB.block_size};

        mgrA.setRefillCallback(&TestRefillCallback, &ctxA);
        mgrA.setReturnCallback(&TestReturnCallback, &ctxA);

        mgrB.setRefillCallback(&TestRefillCallback, &ctxB);
        mgrB.setReturnCallback(&TestReturnCallback, &ctxB);

        p = mgrA.allocateBlock();
        ASSERT_NE(p, nullptr);

        EXPECT_TRUE(mgrA.ownsPointer(p));   // 自己应当认为“可能属于自己”
        EXPECT_FALSE(mgrB.ownsPointer(p));  // 块大小不同 → false

        EXPECT_TRUE(mgrA.releaseBlock(p));
        p = nullptr;
    }

    // 两个 manager 都析构完后再兜底清理
    ctxA.ForceCleanupAll();
    ctxB.ForceCleanupAll();
}
