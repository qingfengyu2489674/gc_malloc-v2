// ManagedList_test.cpp
#include <gtest/gtest.h>

#include "gc_malloc/ThreadHeap/ManagedList.hpp"   // 需要能找到类声明
#include "gc_malloc/ThreadHeap/BlockHeader.hpp"   // 需要能找到 BlockHeader 的实现（.cpp 需参与编译/链接）

// 一个小工具：创建 N 个块并返回指针数组（用 new 保证对齐）
static std::vector<BlockHeader*> make_blocks(std::size_t n, BlockState init = BlockState::Free) {
    std::vector<BlockHeader*> v;
    v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        auto* b = new BlockHeader(init);
        v.push_back(b);
    }
    return v;
}

// 清理工具
static void destroy_blocks(std::vector<BlockHeader*>& v) {
    for (auto* b : v) delete b;
    v.clear();
}

TEST(ManagedListTest, AttachSetsUsedAndTailInsertion) {
    ManagedList ml;

    auto blocks = make_blocks(3, BlockState::Free);
    // attach_used 应将状态改为 Used，并尾插至链表尾
    ml.appendUsed(blocks[0]);
    EXPECT_FALSE(ml.empty());
    EXPECT_EQ(ml.head(), blocks[0]);
    EXPECT_EQ(ml.tail(), blocks[0]);
    EXPECT_EQ(blocks[0]->loadState(), BlockState::Used);
    EXPECT_EQ(blocks[0]->next, nullptr);

    ml.appendUsed(blocks[1]);
    EXPECT_EQ(ml.head(), blocks[0]);
    EXPECT_EQ(ml.tail(), blocks[1]);
    EXPECT_EQ(blocks[0]->next, blocks[1]);
    EXPECT_EQ(blocks[1]->next, nullptr);
    EXPECT_EQ(blocks[1]->loadState(), BlockState::Used);

    ml.appendUsed(blocks[2]);
    EXPECT_EQ(ml.tail(), blocks[2]);
    EXPECT_EQ(blocks[1]->next, blocks[2]);
    EXPECT_EQ(blocks[2]->next, nullptr);
    EXPECT_EQ(blocks[2]->loadState(), BlockState::Used);

    destroy_blocks(blocks);
}

TEST(ManagedListTest, ReclaimRemovesFreedNodesSinglePass) {
    ManagedList ml;
    auto blocks = make_blocks(4, BlockState::Free);

    // 先全部 attach_used（内部置为 Used）
    for (auto* b : blocks) ml.appendUsed(b);

    // 释放中间两个
    blocks[1]->storeFree();
    blocks[2]->storeFree();

    // 开始一轮扫描
    ml.resetCursor();

    // 第一次应回收 blocks[1]
    BlockHeader* r1 = ml.reclaimNextFree();
    ASSERT_NE(r1, nullptr);
    EXPECT_EQ(r1, blocks[1]);
    EXPECT_EQ(r1->next, nullptr);            // 被摘除后应断开
    EXPECT_EQ(ml.head(), blocks[0]);         // 头仍是 blocks[0]
    EXPECT_EQ(ml.head()->next, blocks[2]);   // 链接跳过了 blocks[1]

    // 第二次应回收 blocks[2]
    BlockHeader* r2 = ml.reclaimNextFree();
    ASSERT_NE(r2, nullptr);
    EXPECT_EQ(r2, blocks[2]);
    EXPECT_EQ(r2->next, nullptr);
    EXPECT_EQ(ml.head(), blocks[0]);
    EXPECT_EQ(ml.head()->next, blocks[3]);   // 再次跳过被摘除的 blocks[2]
    EXPECT_EQ(ml.tail(), blocks[3]);

    // 第三次应无可回收
    BlockHeader* r3 = ml.reclaimNextFree();
    EXPECT_EQ(r3, nullptr);

    destroy_blocks(blocks);
}

TEST(ManagedListTest, ResetCursorStartsFromHeadEveryRound) {
    ManagedList ml;
    auto blocks = make_blocks(3, BlockState::Free);
    for (auto* b : blocks) ml.appendUsed(b);

    // Round 1：释放 tail（blocks[2]），扫描应回收它
    blocks[2]->storeFree();
    ml.resetCursor();
    BlockHeader* r1 = ml.reclaimNextFree();
    ASSERT_NE(r1, nullptr);
    EXPECT_EQ(r1, blocks[2]);
    EXPECT_EQ(ml.tail(), blocks[1]);
    EXPECT_EQ(ml.head(), blocks[0]);
    EXPECT_EQ(ml.head()->next, blocks[1]);

    // Round 2：释放 head（blocks[0]），扫描应回收它
    blocks[0]->storeFree();
    ml.resetCursor();
    BlockHeader* r2 = ml.reclaimNextFree();
    ASSERT_NE(r2, nullptr);
    EXPECT_EQ(r2, blocks[0]);
    EXPECT_EQ(ml.head(), blocks[1]);
    EXPECT_EQ(ml.tail(), blocks[1]);
    EXPECT_EQ(ml.head()->next, nullptr);

    // Round 3：无可回收
    BlockHeader* r3 = ml.reclaimNextFree();
    EXPECT_EQ(r3, nullptr);

    destroy_blocks(blocks);
}

TEST(ManagedListTest, ReclaimNoneReturnsNull) {
    ManagedList ml;
    auto blocks = make_blocks(2, BlockState::Free);
    for (auto* b : blocks) ml.appendUsed(b);

    ml.resetCursor();
    // 未将任何块置 Free，第一次回收就应该返回 nullptr
    BlockHeader* r = ml.reclaimNextFree();
    EXPECT_EQ(r, nullptr);

    destroy_blocks(blocks);
}

TEST(ManagedListTest, ReclaimHeadAndTailCorrectlyUpdatePointers) {
    ManagedList ml;
    auto blocks = make_blocks(3, BlockState::Free);
    for (auto* b : blocks) ml.appendUsed(b);

    // 释放 head
    blocks[0]->storeFree();
    ml.resetCursor();
    BlockHeader* r1 = ml.reclaimNextFree();
    ASSERT_NE(r1, nullptr);
    EXPECT_EQ(r1, blocks[0]);
    EXPECT_EQ(ml.head(), blocks[1]);
    EXPECT_EQ(ml.tail(), blocks[2]);
    EXPECT_EQ(ml.head()->next, blocks[2]);

    // 释放 tail
    blocks[2]->storeFree();
    // 注意：继续沿用当前游标也可，但我们模拟新一轮回收
    ml.resetCursor();
    BlockHeader* r2 = ml.reclaimNextFree();
    ASSERT_NE(r2, nullptr);
    EXPECT_EQ(r2, blocks[2]);
    EXPECT_EQ(ml.head(), blocks[1]);
    EXPECT_EQ(ml.tail(), blocks[1]);
    EXPECT_EQ(ml.head()->next, nullptr);

    destroy_blocks(blocks);
}
