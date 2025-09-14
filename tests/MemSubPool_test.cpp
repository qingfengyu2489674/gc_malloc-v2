#include "gtest/gtest.h"
#include "gc_malloc/ThreadHeap/MemSubPool.hpp" // 引入您确定的 MemSubPool 头文件
#include <vector>
#include <thread>
#include <numeric> // for std::iota

// 创建一个测试夹具(Test Fixture)来管理内存池的生命周期
class MemSubPoolTest : public ::testing::Test {
protected:
    // 在每个测试用例开始前执行
    void SetUp() override {
        // 1. 模拟 CentralHeap 分配一块 2MB 的对齐内存
        raw_memory_ = ::operator new(MemSubPool::kPoolTotalSize,
                                     std::align_val_t(MemSubPool::kPoolAlignment));
        ASSERT_NE(raw_memory_, nullptr);

        // 2. 在这块内存上使用 placement new 构造 MemSubPool 对象
        pool_ = new (raw_memory_) MemSubPool(block_size_);
        ASSERT_NE(pool_, nullptr);
    }

    // 在每个测试用例结束后执行
    void TearDown() override {
        if (pool_) {
            pool_->~MemSubPool();
        }
        if (raw_memory_) {
            ::operator delete(raw_memory_, std::align_val_t(MemSubPool::kPoolAlignment));
        }
    }

    // 成员变量
    void* raw_memory_ = nullptr;
    static constexpr size_t block_size_ = 128; // 为测试选择一个块大小
    MemSubPool* pool_ = nullptr;
};

// --- 测试用例 ---

// 测试构造后，内存池的初始状态是否正确
TEST_F(MemSubPoolTest, InitialStateIsCorrect) {
    EXPECT_EQ(pool_->GetBlockSize(), block_size_);
    EXPECT_TRUE(pool_->IsEmpty());
    EXPECT_FALSE(pool_->IsFull());
}

// 测试基本的分配和释放功能
TEST_F(MemSubPoolTest, HandlesSingleAllocationAndRelease) {
    ASSERT_TRUE(pool_->IsEmpty());

    // 分配一个块
    void* block = pool_->Allocate();
    ASSERT_NE(block, nullptr);
    
    // 分配后，池不应为空
    EXPECT_FALSE(pool_->IsEmpty());

    // 释放这个块
    pool_->Release(block);

    // 释放后，池应该再次变为空
    EXPECT_TRUE(pool_->IsEmpty());
}

// 测试将内存池分配满的情况
TEST_F(MemSubPoolTest, HandlesFullAllocation) {
    std::vector<void*> allocated_blocks;
    
    // 持续分配，直到 Allocate 返回 nullptr
    while(true) {
        void* block = pool_->Allocate();
        if (block == nullptr) {
            break; // 池已满
        }
        allocated_blocks.push_back(block);
    }

    // 此时，池应该是满的，但不应该是空的
    EXPECT_TRUE(pool_->IsFull());
    EXPECT_FALSE(pool_->IsEmpty());
    
    // 确认再次分配会失败
    EXPECT_EQ(pool_->Allocate(), nullptr);

    // 释放所有块
    for (void* block : allocated_blocks) {
        pool_->Release(block);
    }

    // 释放后，池应该是空的，但不应该是满的
    EXPECT_TRUE(pool_->IsEmpty());
    EXPECT_FALSE(pool_->IsFull());
}

// 一个非常简单的多线程测试，检查是否有数据竞争问题
TEST_F(MemSubPoolTest, IsThreadSafe) {
    const size_t num_threads = 8;
    // 分配数量要足够小，确保池不会被填满
    const size_t allocations_per_thread = 50; 
    
    auto worker_task = [this]() {
        std::vector<void*> local_blocks;
        local_blocks.reserve(allocations_per_thread);
        
        // 分配阶段
        for (size_t j = 0; j < allocations_per_thread; ++j) {
            void* block = pool_->Allocate();
            if (block) {
                local_blocks.push_back(block);
            }
        }
        
        // 释放阶段
        for (void* block : local_blocks) {
            pool_->Release(block);
        }
    };

    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker_task);
    }
    
    for (auto& t : threads) {
        t.join();
    }

    // 所有线程结束后，内存池应该 kembali为空
    EXPECT_TRUE(pool_->IsEmpty()) << "Pool should be empty after all threads finished.";
}