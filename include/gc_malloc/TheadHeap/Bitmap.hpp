#pragma once

#include <cstddef>
#include <cstdint>


class Bitmap {
public:

    explicit Bitmap(size_t capacity_in_bits, unsigned char* buffer, size_t buffer_size_in_bytes);

    void MarkAsUsed(size_t bit_index);

    void MarkAsFree(size_t bit_index);

    bool IsUsed(size_t bit_index) const;

    size_t FindFirstFree(size_t start_bit = 0) const;

    static constexpr size_t kNotFound = static_cast<size_t>(-1);

private:
    unsigned char* buffer_;       // 指向外部提供的位图内存缓冲区
    const size_t capacity_in_bits_; // 此位图管理的有效位数
};