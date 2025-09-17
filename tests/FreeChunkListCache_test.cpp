#include "gtest/gtest.h"
#include "gc_malloc/CentralHeap/FreeChunkListCache.hpp" // 引入要测试的类

#include <thread>
#include <vector>
#include <type_traits> // For std::aligned_storage

// 定义一个测试固件，保持不变
class FreeChunkListCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = new FreeChunkListCache();
    }

    void TearDown() override {
        // 【修改】为多线程测试清理动态分配的内存
        for (void* chunk : multithread_chunks_) {
            delete[] static_cast<char*>(chunk);
        }
        delete cache;
    }

    FreeChunkListCache* cache;
    // 【新增】用于在 TearDown 中统一清理多线程测试的内存
    std::vector<void*> multithread_chunks_;
};

// 测试1：初始状态，保持不变
TEST_F(FreeChunkListCacheTest, InitialStateIsEmpty) {
    ASSERT_EQ(cache->getCacheCount(), 0);
    ASSERT_EQ(cache->acquire(), nullptr);
}

// 测试2：基本流程，修改了 dummy_chunk 的创建方式
TEST_F(FreeChunkListCacheTest, DepositAndAcquireSingleChunk) {
    // 【修改】创建一个足够大且对齐的缓冲区来模拟内存块
    // 确保它至少能容纳一个指针
    std::aligned_storage<sizeof(void*)>::type dummy_chunk;
    void* ptr = &dummy_chunk;

    cache->deposit(ptr);
    ASSERT_EQ(cache->getCacheCount(), 1);

    void* acquired_ptr = cache->acquire();
    ASSERT_EQ(acquired_ptr, ptr);
    ASSERT_EQ(cache->getCacheCount(), 0);
    ASSERT_EQ(cache->acquire(), nullptr);
}

// 测试3：LIFO 顺序，修改了 dummy_chunk 的创建方式
TEST_F(FreeChunkListCacheTest, LIFO_OrderIsCorrect) {
    // 【修改】同样，使用足够大的缓冲区
    std::aligned_storage<sizeof(void*)>::type dummy_chunk1;
    std::aligned_storage<sizeof(void*)>::type dummy_chunk2;
    void* ptr1 = &dummy_chunk1;
    void* ptr2 = &dummy_chunk2;

    cache->deposit(ptr1);
    cache->deposit(ptr2);

    ASSERT_EQ(cache->getCacheCount(), 2);
    ASSERT_EQ(cache->acquire(), ptr2);
    ASSERT_EQ(cache->acquire(), ptr1);
    ASSERT_EQ(cache->getCacheCount(), 0);
}

// 测试4：存入 nullptr，保持不变
TEST_F(FreeChunkListCacheTest, DepositNullptrIsIgnored) {
    cache->deposit(nullptr);
    ASSERT_EQ(cache->getCacheCount(), 0);
}


// 测试6：线程安全测试，修改了测试数据的创建方式
TEST_F(FreeChunkListCacheTest, IsThreadSafeUnderConcurrentAccess) {
    const int num_items = 10000;
    
    // 【修改】创建真实的、足够大的内存块用于测试
    // 并将它们存入成员变量以便在 TearDown 中释放
    multithread_chunks_.reserve(num_items);
    for (int i = 0; i < num_items; ++i) {
        // 在测试代码中使用 new 是可以的
        multithread_chunks_.push_back(new char[sizeof(void*)]);
    }

    std::thread producer([this]() {
        for (void* chunk : multithread_chunks_) {
            cache->deposit(chunk);
        }
    });

    std::vector<void*> acquired_chunks;
    acquired_chunks.reserve(num_items);
    std::thread consumer([this, &acquired_chunks, num_items]() {
        for (int i = 0; i < num_items; ) {
            void* chunk = cache->acquire();
            if (chunk != nullptr) {
                acquired_chunks.push_back(chunk);
                i++;
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(cache->getCacheCount(), 0);
    ASSERT_EQ(acquired_chunks.size(), num_items);
}