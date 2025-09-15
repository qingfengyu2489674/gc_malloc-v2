#pragma once

#include <cstdint>
#include <atomic>


enum class BlockState : std::uint64_t {
    Free = 0,
    Used = 1
};


// 块头部：前 16 字节 = [8B 链表指针][8B 状态位]
struct alignas(16) BlockHeader {
    BlockHeader* next;                  // 8B：单链表指针
    std::atomic<std::uint64_t>  state;  // 8B：状态位（原子或volatile）

    // ---- 构造与析构（仅声明）----
    BlockHeader() noexcept;
    explicit BlockHeader(BlockState s) noexcept;
    ~BlockHeader() = default;

    BlockState load_state() const noexcept;

    void store_free() noexcept;

    void store_used() noexcept;
};
