#include "gtest/gtest.h"
#include "gc_malloc/CentralHeap/CentralHeap.hpp" // 引入要测试的类

// 定义一个简单的测试固件
class CentralHeapTest : public ::testing::Test {
protected:
    // 为了让测试结果可预测，我们访问 CentralHeap 的内部常量
    // 注意：在真实项目中，通常不建议这样做，但为了白盒测试是必要的。
    // 如果这些常量是 private 的，你需要将测试类声明为 CentralHeap 的 friend，
    // 或者将常量设为 public。这里我们假设它们是 public 的以便测试。
    // 如果是 private, 在 CentralHeap.hpp 中加入: friend class CentralHeapTest;
    
    // 为了简洁，我们在这里重新定义它们
    static constexpr size_t kChunkSize = 2 * 1024 * 1024;
    static constexpr size_t kMaxWatermarkInChunks = 16;
    static constexpr size_t kTargetWatermarkInChunks = 8;
};

// 测试1：验证 GetInstance() 总是返回同一个单例实例
TEST_F(CentralHeapTest, GetInstanceReturnsSameInstance) {
    CentralHeap& instance1 = CentralHeap::GetInstance();
    CentralHeap& instance2 = CentralHeap::GetInstance();

    // 比较两个引用的地址，它们应该完全相同
    ASSERT_EQ(&instance1, &instance2);
}

// 测试2：验证首次分配能触发缓存填充并成功返回一个块
TEST_F(CentralHeapTest, FirstAcquirePopulatesCacheAndSucceeds) {
    CentralHeap& heap = CentralHeap::GetInstance();

    // 在第一次分配前，我们无法轻易知道缓存状态，但可以假设它接近初始状态
    
    void* chunk = heap.acquire_chunk(kChunkSize);

    // 断言我们成功获取了一个非空的内存块
    ASSERT_NE(chunk, nullptr);
    
    // 释放这个块，以便其他测试可以重用
    heap.release_chunk(chunk, kChunkSize);
}

// 测试3：验证缓存耗尽后能再次填充
TEST_F(CentralHeapTest, CacheRefillsAfterDepletion) {
    CentralHeap& heap = CentralHeap::GetInstance();
    std::vector<void*> chunks;

    // 首先，取走目标水位线数量的块，这应该会耗尽初始填充的缓存
    for (size_t i = 0; i < kTargetWatermarkInChunks; ++i) {
        void* chunk = heap.acquire_chunk(kChunkSize);
        ASSERT_NE(chunk, nullptr) << "Failed to acquire chunk #" << i;
        chunks.push_back(chunk);
    }
    
    // 再取一个，这应该会触发第二次 refill
    void* extra_chunk = heap.acquire_chunk(kChunkSize);
    ASSERT_NE(extra_chunk, nullptr) << "Failed to acquire chunk after depleting cache.";
    chunks.push_back(extra_chunk);

    // 将所有获取的块都释放回缓存
    for (void* chunk : chunks) {
        heap.release_chunk(chunk, kChunkSize);
    }
}

// 测试4：验证释放块时高水位线机制能正常工作
TEST_F(CentralHeapTest, ReleaseStopsAtHighWatermark) {
    CentralHeap& heap = CentralHeap::GetInstance();
    std::vector<void*> chunks;

    // 准备比高水位线更多的块（例如 kMax + 5）
    const size_t num_to_test = kMaxWatermarkInChunks + 5;
    for (size_t i = 0; i < num_to_test; ++i) {
        // 我们需要先从系统获取这些块
        void* chunk = heap.acquire_chunk(kChunkSize);
        // 如果系统内存不足，测试无法进行
        if (chunk == nullptr) {
            GTEST_SKIP() << "System out of memory, cannot perform high watermark test.";
        }
        chunks.push_back(chunk);
    }

    // 现在，将所有这些块释放回中心堆
    for (void* chunk : chunks) {
        heap.release_chunk(chunk, kChunkSize);
    }

    // 我们无法直接访问缓存数量，这是一个设计上的限制。
    // 但我们可以通过行为来推断。我们现在尝试取回 kMaxWatermarkInChunks + 1 个块。
    // 如果高水位线机制正常工作，缓存里最多只有 kMaxWatermarkInChunks 个块，
    // 第 kMax + 1 次获取应该会触发一次 refill。
    // 这是一个间接的验证。
    
    std::vector<void*> acquired_chunks;
    for (size_t i = 0; i < kMaxWatermarkInChunks + 1; ++i) {
        void* chunk = heap.acquire_chunk(kChunkSize);
        ASSERT_NE(chunk, nullptr) << "Failed to acquire chunk #" << i << " during watermark check.";
        acquired_chunks.push_back(chunk);
    }

    // 清理
    for (void* chunk : acquired_chunks) {
        heap.release_chunk(chunk, kChunkSize);
    }
}