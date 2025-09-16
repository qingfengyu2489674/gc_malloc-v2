#include "gtest/gtest.h"

#include "gc_malloc/ThreadHeap/ThreadHeap.hpp"
#include "gc_malloc/ThreadHeap/BlockHeader.hpp"
#include "gc_malloc/ThreadHeap/SizeClassConfig.hpp"

// 仅测试小对象路径
TEST(ThreadHeapTest, AllocateDeallocateSmallObject) {
    constexpr std::size_t req_size = 64; // 小对象
    void* p = ThreadHeap::Allocate(req_size);
    ASSERT_NE(p, nullptr) << "Allocate should succeed for small object";

    auto* hdr = static_cast<BlockHeader*>(p);
    // 新分配的块应为 Used
    EXPECT_EQ(hdr->load_state(), BlockState::Used);

    // 释放后应标记为 Free（跨线程安全：这里只是标位）
    ThreadHeap::Deallocate(p);
    EXPECT_EQ(hdr->load_state(), BlockState::Free);

    // 当前线程执行一次 GC，应回收这一个块
    std::size_t reclaimed = ThreadHeap::GarbageCollect();
    EXPECT_EQ(reclaimed, 1u);
}

TEST(ThreadHeapTest, MultipleSmallAllocations) {
    constexpr std::size_t req_size = 128;

    void* p1 = ThreadHeap::Allocate(req_size);
    void* p2 = ThreadHeap::Allocate(req_size);

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p1, p2) << "Two small allocations should yield different blocks";

    auto* h1 = static_cast<BlockHeader*>(p1);
    auto* h2 = static_cast<BlockHeader*>(p2);

    EXPECT_EQ(h1->load_state(), BlockState::Used);
    EXPECT_EQ(h2->load_state(), BlockState::Used);

    ThreadHeap::Deallocate(p2);
    ThreadHeap::Deallocate(p1);

    EXPECT_EQ(h1->load_state(), BlockState::Free);
    EXPECT_EQ(h2->load_state(), BlockState::Free);

    std::size_t reclaimed = ThreadHeap::GarbageCollect();
    EXPECT_EQ(reclaimed, 2u);
}
