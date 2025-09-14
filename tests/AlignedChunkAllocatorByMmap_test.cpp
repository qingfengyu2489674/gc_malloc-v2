#include "gtest/gtest.h"
#include "gc_malloc/CentralHeap/AlignedChunkAllocatorByMmap.hpp"

// 用于测试的对齐尺寸，应与类内部的定义保持一致。
constexpr size_t kAlignmentSize = 2 * 1024 * 1024;

// 定义一个新的测试固件 (Test Fixture)，使用新的类名
class AlignedChunkAllocatorByMmapTest : public ::testing::Test {
protected:
    // SetUp 在每个 TEST_F 运行前被调用
    void SetUp() override {
        // 【修改核心】不再使用单例，而是为每个测试创建一个新的实例
        allocator = new AlignedChunkAllocatorByMmap();
    }

    // TearDown 在每个 TEST_F 运行后被调用
    void TearDown() override {
        // 【修改核心】清理为测试创建的实例，防止内存泄漏
        delete allocator;
    }

    // 使用新的类名和成员变量名
    AlignedChunkAllocatorByMmap* allocator;
};

// --- 1. 测试成功路径 ---
TEST_F(AlignedChunkAllocatorByMmapTest, AllocateAndDeallocateSuccessfully) {
    const size_t alloc_size = 2 * kAlignmentSize; // 分配 4MB
    
    void* ptr = allocator->allocate(alloc_size);

    // 【修改核心】移除对系统大页的依赖检查。
    // mmap 失败现在只可能是通用原因（如内存不足）。
    ASSERT_NE(ptr, nullptr) << "mmap failed. The system may be out of memory.";

    // 验证返回的指针是2MB对齐的
    ASSERT_EQ(reinterpret_cast<uintptr_t>(ptr) & (kAlignmentSize - 1), 0);

    // 成功释放，不应该崩溃或触发断言
    ASSERT_NO_THROW(allocator->deallocate(ptr, alloc_size));
}


// --- 2. 测试触发 allocate 的断言 ---

TEST_F(AlignedChunkAllocatorByMmapTest, AllocateFailsWithZeroSize) {
    // 【修改】更新断言消息中的常量名
    ASSERT_DEATH({
        allocator->allocate(0);
    }, "Allocation size must be a positive multiple of kAlignmentSize");
}

TEST_F(AlignedChunkAllocatorByMmapTest, AllocateFailsWithNonMultipleSize) {
    // 【修改】更新断言消息中的常量名
    ASSERT_DEATH({
        allocator->allocate(kAlignmentSize + 1);
    }, "Allocation size must be a positive multiple of kAlignmentSize");
}


// --- 3. 测试触发 deallocate 的断言 ---

TEST_F(AlignedChunkAllocatorByMmapTest, DeallocateFailsWithNullPtr) {
    ASSERT_DEATH({
        allocator->deallocate(nullptr, kAlignmentSize);
    }, "Cannot deallocate a null pointer");
}

TEST_F(AlignedChunkAllocatorByMmapTest, DeallocateFailsWithZeroSize) {
    void* ptr = allocator->allocate(kAlignmentSize);
    // 如果 mmap 失败，这个测试就无法进行，直接跳过
    if (ptr == nullptr) {
        GTEST_SKIP() << "Could not allocate memory for the test setup.";
    }

    ASSERT_DEATH({
        allocator->deallocate(ptr, 0);
    }, "Deallocation size must be positive");
    
    // 清理在死亡测试之外分配的内存，防止内存泄漏
    allocator->deallocate(ptr, kAlignmentSize);
}

TEST_F(AlignedChunkAllocatorByMmapTest, DeallocateFailsWhenMunmapFails) {
    void* ptr = allocator->allocate(kAlignmentSize);
    if (ptr == nullptr) {
        GTEST_SKIP() << "Could not allocate memory for the test setup.";
    }

    // munmap 失败的一种方式是传入的地址不是一个有效映射的起始地址。
    // ptr+1 肯定不是。这会让 munmap 返回 -1，并触发我们代码中的 assert。
    void* invalid_ptr = reinterpret_cast<void*>(reinterpret_cast<char*>(ptr) + 1);

    ASSERT_DEATH({
        allocator->deallocate(invalid_ptr, kAlignmentSize);
    }, "munmap failed!");

    // 清理我们最开始正确分配的内存
    allocator->deallocate(ptr, kAlignmentSize);
}