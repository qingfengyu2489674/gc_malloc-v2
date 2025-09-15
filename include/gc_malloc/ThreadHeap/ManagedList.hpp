#pragma once

#include <cstdint>
#include "BlockHeader.hpp"

// 托管链表：管理“已分配出去”的块（BlockHeader）
// 特性：单线程回收遍历，无需加锁。
// 约定：push_back 时会将 blk->next 置空并尾插到链表；
//       回收遍历时，若发现块已释放（state==Free），则自链表摘除并返回该块。

class ManagedList {
public:
    ManagedList() noexcept;
    ~ManagedList() = default;

    ManagedList(const ManagedList&) = delete;
    ManagedList& operator=(const ManagedList&) = delete;
    ManagedList(ManagedList&&) = delete;
    ManagedList& operator=(ManagedList&&) = delete;

    // 1) 尾插：将内存块以尾插法插入链表（blk 非空，且可被重用其 next 指针）
    void push_back(BlockHeader* blk) noexcept;

    // 2) 从“游标”位置开始遍历：
    //    - 跳过未释放的块；
    //    - 一旦遇到已释放的块（state == BlockState::Free），将其从链表移除并返回；
    //    - 若遍历至末尾未发现可回收块，返回 nullptr。
    // 说明：本过程单线程执行，不会出现竞态，不加锁。
    BlockHeader* next_reclaimed() noexcept;

    // 3) 重置游标到链表头部（用于开始新一轮垃圾回收遍历）
    void reset_cursor() noexcept;

    // 可选：调试/状态查询（仅声明，不要求实现）
    bool empty() const noexcept;
    BlockHeader* head() const noexcept;
    BlockHeader* tail() const noexcept;

private:
    // 链表头尾
    BlockHeader* head_ = nullptr;
    BlockHeader* tail_ = nullptr;

    // 遍历游标：维护“前驱”和“当前”，便于 O(1) 摘除当前节点
    BlockHeader* cursor_prev_ = nullptr;
    BlockHeader* cursor_cur_  = nullptr;
};
