#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <new>

namespace mpool {

// 缓存行大小，用于防止多核环境下的伪共享
constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * @class MemorySubPool
 * @brief 一个线程安全的、固定大小(2MB)的内存子池，带有局部缓存以提升性能。
 *        此类本身即是内存池的元数据，会被直接放置在内存块的起始位置。
 */
class alignas(CACHE_LINE_SIZE) MemorySubPool {
public:
    // --- 常量定义 ---
    static constexpr size_t POOL_TOTAL_SIZE = 2 * 1024 * 1024;
    static constexpr size_t POOL_ALIGNMENT = POOL_TOTAL_SIZE;
    static constexpr uint32_t POOL_MAGIC = 0xDEADBEEF;

    // --- 静态工厂与析构方法 ---

    /**
     * @brief 创建并初始化一个新的内存子池实例。
     * @param block_size 每个内存块的大小 (例如: 32, 64, 128...)。
     * @param cache_watermark 内部缓存的水位线, 0表示禁用缓存。
     * @return 指向新内存子池的指针。
     */
    static MemorySubPool* create(size_t block_size, size_t cache_watermark = 128);

    /**
     * @brief 销毁内存子池实例并释放其占用的内存。
     */
    static void destroy(MemorySubPool* pool);

    /**
     * @brief 释放一个内存块回其所属的内存子池。
     */
    static void release(void* block_ptr);

    // --- 实例方法 ---

    void* allocate();
    size_t get_allocated_count() const;
    size_t get_total_block_count() const;
    size_t get_cache_size() const;

private:
    // --- 私有类型与辅助函数 ---
    
    // 前向声明一个私有的初始化参数结构体，实现细节隐藏在.cpp文件中。
    struct PoolInitParams;

    // --- 私有构造与析构 ---
    explicit MemorySubPool(const PoolInitParams& params);
    ~MemorySubPool();

    // --- 内部实现方法 ---
    void deallocate_internal(void* block_ptr);
    void return_block_to_bitmap(void* block_ptr);
    unsigned char* get_bitmap();

    // --- 成员变量 (元数据) ---
    // 所有元数据作为类的私有成员，实现了完全封装。
    
    // 核心标识与锁
    uint32_t magic; // 魔数，必须是第一个成员，用于快速校验
    char pad1[CACHE_LINE_SIZE - sizeof(uint32_t)];
    std::mutex lock;

    // 布局信息 (创建时设置，不可变)
    const size_t pool_total_size;
    const size_t block_size;
    const size_t num_blocks;
    const size_t data_offset;

    // 位图分配状态
    size_t next_free_block_hint;
    std::atomic<size_t> num_allocated_from_bitmap;

    // 缓存机制
    void* cache_head;
    const size_t cache_watermark;
    std::atomic<size_t> cache_current_size;

    // --- 禁用函数 ---
    MemorySubPool(const MemorySubPool&) = delete;
    MemorySubPool& operator=(const MemorySubPool&) = delete;
    MemorySubPool(MemorySubPool&&) = delete;
    MemorySubPool& operator=(MemorySubPool&&) = delete;
};

} // namespace mpool



#include "memory_sub_pool.hpp"
#include <cstring> // for memset
#include <stdexcept> // for exceptions

namespace mpool {

// 1. 定义私有的初始化参数结构体
//    它的作用是打包所有需要在构造函数初始化列表中使用的最终值。
struct MemorySubPool::PoolInitParams {
    size_t block_size;
    size_t cache_watermark;
    size_t num_blocks;
    size_t data_offset;
};

// 2. 定义私有的静态辅助函数，用于执行所有复杂的布局计算
//    这是整个初始化流程的核心，它打破了循环依赖。
static PoolInitParams calculate_initialization_params(size_t block_size, size_t cache_watermark) {
    if (block_size == 0) {
        throw std::invalid_argument("Block size cannot be zero.");
    }

    PoolInitParams params;
    params.block_size = block_size;
    params.cache_watermark = cache_watermark;

    // 2.1.【预估】: 假设除了元数据全是数据区，计算出一个“过量”的位图大小
    const size_t max_available_space = MemorySubPool::POOL_TOTAL_SIZE - sizeof(MemorySubPool);
    const size_t estimated_max_blocks = max_available_space / block_size;
    const size_t bitmap_size_in_bytes = (estimated_max_blocks + 7) / 8;

    // 2.2.【精算 data_offset】: 根据元数据大小和“过量”位图大小，计算出精确的数据区偏移
    const size_t start_of_data_area = sizeof(MemorySubPool) + bitmap_size_in_bytes;
    const size_t alignment = alignof(std::max_align_t);
    params.data_offset = (start_of_data_area + alignment - 1) & ~(alignment - 1);

    // 2.3.【精算 num_blocks】: 根据精确的数据区偏移，计算出最终能容纳的块数量
    const size_t data_area_size = MemorySubPool::POOL_TOTAL_SIZE - params.data_offset;
    params.num_blocks = data_area_size / block_size;

    return params;
}

// 3. 实现工厂方法 create
MemorySubPool* MemorySubPool::create(size_t block_size, size_t cache_watermark) {
    // 3.1. 调用辅助函数，一次性准备好所有初始化参数
    const PoolInitParams params = calculate_initialization_params(block_size, cache_watermark);
    
    // 3.2. 从底层获取原始内存
    void* raw_memory = ::operator new(POOL_TOTAL_SIZE, std::align_val_t(POOL_ALIGNMENT));
    if (!raw_memory) {
        throw std::bad_alloc();
    }
    
    // 3.3. 使用 placement new 和准备好的参数来构造对象
    return new (raw_memory) MemorySubPool(params);
}

// 4. 实现私有构造函数
//    它的初始化列表现在非常简洁，只负责从准备好的参数包中“装配”成员变量。
MemorySubPool::MemorySubPool(const PoolInitParams& params)
    : magic(POOL_MAGIC),
      pad1{0},
      pool_total_size(POOL_TOTAL_SIZE),
      block_size(params.block_size),
      num_blocks(params.num_blocks),
      data_offset(params.data_offset),
      cache_watermark(params.cache_watermark),
      next_free_block_hint(0),
      num_allocated_from_bitmap(0),
      cache_head(nullptr),
      cache_current_size(0)
{
    // 构造函数体只负责最后的收尾工作，比如清空位图
    std::memset(get_bitmap(), 0, (num_blocks + 7) / 8);
}


// ... 其他函数的实现（destroy, release, allocate 等）...

} // namespace mpool