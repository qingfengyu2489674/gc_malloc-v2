#pragma once

#include <cstddef>

class MemSubPool;

// 侵入式双向链表（节点即 MemSubPool），只提供：头插、弹头、按节点摘除并返回。
// 不做任何内存分配；不保证跨线程并发。
class MemSubPoolList {
public:
    MemSubPoolList() noexcept;
    ~MemSubPoolList() = default;

    MemSubPoolList(const MemSubPoolList&)            = delete;
    MemSubPoolList& operator=(const MemSubPoolList&) = delete;
    MemSubPoolList(MemSubPoolList&&)                 = delete;
    MemSubPoolList& operator=(MemSubPoolList&&)      = delete;

    // 基本查询
    bool        empty() const noexcept;
    std::size_t size()  const noexcept;
    MemSubPool* front() const noexcept;   // 可能为 nullptr

    // 修改操作（全部 O(1)）
    void        push_front(MemSubPool* node) noexcept; // 头插；要求 node 当前未在任一链表中
    MemSubPool* pop_front() noexcept;                  // 弹出并返回链表头；空则返回 nullptr
    MemSubPool* remove(MemSubPool* node) noexcept;     // 从本链表中摘除并返回 node；若不在本链表中行为未定义
    
private:
    MemSubPool* head_ = nullptr;
    MemSubPool* tail_ = nullptr;
    std::size_t size_ = 0;
};
