// tests/MemSubPoolList_test.cpp

#include "gtest/gtest.h"

#include "gc_malloc/ThreadHeap/MemSubPoolList.hpp"
#include "gc_malloc/ThreadHeap/MemSubPool.hpp"

// 简单的节点工厂：只构造 MemSubPool，不做任何 Allocate/Release
static MemSubPool* MakeNode(size_t block_sz, void* storage = nullptr) {
    if (storage) {
        return new (storage) MemSubPool(block_sz);
    }
    // 直接栈上构造（注意：不要对该对象调用 Allocate/Release）
    return new MemSubPool(block_sz);
}

TEST(MemSubPoolList, PushFrontOrderAndFront) {
    MemSubPoolList list;

    // 三个节点（栈上 new，测试结束后 delete）
    auto* a = MakeNode(64);
    auto* b = MakeNode(64);
    auto* c = MakeNode(64);

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0u);
    EXPECT_EQ(list.front(), nullptr);

    list.pusFront(a);
    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.size(), 1u);
    EXPECT_EQ(list.front(), a);
    EXPECT_EQ(a->list_prev, nullptr);
    EXPECT_EQ(a->list_next, nullptr);

    list.pusFront(b); // 头插 -> b,a
    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(list.front(), b);
    EXPECT_EQ(b->list_prev, nullptr);
    EXPECT_EQ(b->list_next, a);
    EXPECT_EQ(a->list_prev, b);

    list.pusFront(c); // 头插 -> c,b,a
    EXPECT_EQ(list.size(), 3u);
    EXPECT_EQ(list.front(), c);
    EXPECT_EQ(c->list_prev, nullptr);
    EXPECT_EQ(c->list_next, b);
    EXPECT_EQ(b->list_prev, c);
    EXPECT_EQ(b->list_next, a);
    EXPECT_EQ(a->list_prev, b);

    delete a;
    delete b;
    delete c;
}

TEST(MemSubPoolList, PopFrontUnlinksAndReturnsInLIFOHeadOrder) {
    MemSubPoolList list;

    auto* a = MakeNode(64);
    auto* b = MakeNode(64);
    auto* c = MakeNode(64);

    list.pusFront(a); // a
    list.pusFront(b); // b,a
    list.pusFront(c); // c,b,a

    // 弹 c
    {
        MemSubPool* x = list.popFront();
        EXPECT_EQ(x, c);
        EXPECT_EQ(list.size(), 2u);
        EXPECT_EQ(list.front(), b);
        // 被摘除节点应当被清空链表指针
        EXPECT_EQ(c->list_prev, nullptr);
        EXPECT_EQ(c->list_next, nullptr);
    }

    // 弹 b
    {
        MemSubPool* x = list.popFront();
        EXPECT_EQ(x, b);
        EXPECT_EQ(list.size(), 1u);
        EXPECT_EQ(list.front(), a);
        EXPECT_EQ(b->list_prev, nullptr);
        EXPECT_EQ(b->list_next, nullptr);
    }

    // 弹 a -> 空
    {
        MemSubPool* x = list.popFront();
        EXPECT_EQ(x, a);
        EXPECT_EQ(list.size(), 0u);
        EXPECT_TRUE(list.empty());
        EXPECT_EQ(list.front(), nullptr);
        EXPECT_EQ(a->list_prev, nullptr);
        EXPECT_EQ(a->list_next, nullptr);
    }

    // 空表再弹
    EXPECT_EQ(list.popFront(), nullptr);

    delete a;
    delete b;
    delete c;
}

TEST(MemSubPoolList, RemoveHeadMiddleTailAndSingle) {
    MemSubPoolList list;

    auto* a = MakeNode(64);
    auto* b = MakeNode(64);
    auto* c = MakeNode(64);
    auto* d = MakeNode(64);

    // 头插顺序：d,c,b,a
    list.pusFront(a);
    list.pusFront(b);
    list.pusFront(c);
    list.pusFront(d);
    ASSERT_EQ(list.size(), 4u);
    ASSERT_EQ(list.front(), d);

    // 移除中间节点：b（当前链：d,c,b,a）
    {
        MemSubPool* ret = list.remove(b);
        EXPECT_EQ(ret, b);
        EXPECT_EQ(list.size(), 3u);
        // d <-> c <-> a
        EXPECT_EQ(d->list_prev, nullptr);
        EXPECT_EQ(d->list_next, c);
        EXPECT_EQ(c->list_prev, d);
        EXPECT_EQ(c->list_next, a);
        EXPECT_EQ(a->list_prev, c);
        EXPECT_EQ(a->list_next, nullptr);
        // 被摘除节点应当清空 link
        EXPECT_EQ(b->list_prev, nullptr);
        EXPECT_EQ(b->list_next, nullptr);
    }

    // 移除头结点：d（当前链：d,c,a）
    {
        MemSubPool* ret = list.remove(d);
        EXPECT_EQ(ret, d);
        EXPECT_EQ(list.size(), 2u);
        // c <-> a
        EXPECT_EQ(list.front(), c);
        EXPECT_EQ(c->list_prev, nullptr);
        EXPECT_EQ(c->list_next, a);
        EXPECT_EQ(a->list_prev, c);
        EXPECT_EQ(a->list_next, nullptr);
        EXPECT_EQ(d->list_prev, nullptr);
        EXPECT_EQ(d->list_next, nullptr);
    }

    // 移除尾结点：a（当前链：c,a）
    {
        MemSubPool* ret = list.remove(a);
        EXPECT_EQ(ret, a);
        EXPECT_EQ(list.size(), 1u);
        // c
        EXPECT_EQ(list.front(), c);
        EXPECT_EQ(c->list_prev, nullptr);
        EXPECT_EQ(c->list_next, nullptr);
        EXPECT_EQ(a->list_prev, nullptr);
        EXPECT_EQ(a->list_next, nullptr);
    }

    // 只剩一个：c，移除单节点
    {
        MemSubPool* ret = list.remove(c);
        EXPECT_EQ(ret, c);
        EXPECT_EQ(list.size(), 0u);
        EXPECT_TRUE(list.empty());
        EXPECT_EQ(list.front(), nullptr);
        EXPECT_EQ(c->list_prev, nullptr);
        EXPECT_EQ(c->list_next, nullptr);
    }

    delete a;
    delete b;
    delete c;
    delete d;
}
