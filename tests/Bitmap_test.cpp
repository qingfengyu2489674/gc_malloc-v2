#include "gtest/gtest.h"
#include "gc_malloc/ThreadHeap/Bitmap.hpp"

// 测试套件名称: BitmapTest
// 测试用例名称: HandlesInitializationCorrectly
TEST(BitmapTest, HandlesInitializationCorrectly) {
    const size_t capacity = 20; // 需要 3 字节
    const size_t buffer_size = 5; // 预留 5 字节
    unsigned char buffer[buffer_size];
    
    // 调用构造函数
    Bitmap bitmap(capacity, buffer, buffer_size);

    // 检查有效字节 (0 = free)
    EXPECT_EQ(buffer[0], 0x00);
    EXPECT_EQ(buffer[1], 0x00);
    
    // 最后一个有效字节(索引2)，容量为 20%8=4 位。
    // 有效位 0,1,2,3 应为0。无效位 4,5,6,7 应为1。
    // 最终字节为 11110000 (二进制) = 0xF0 (十六进制)
    EXPECT_EQ(buffer[2], 0xF0);

    // 检查超出有效范围的预留字节 (应该全为 1)
    EXPECT_EQ(buffer[3], 0xFF);
    EXPECT_EQ(buffer[4], 0xFF);
}

// 测试套件名称: BitmapTest
// 测试用例名称: HandlesBasicOperations
TEST(BitmapTest, HandlesBasicOperations) {
    const size_t capacity = 16;
    unsigned char buffer[2];
    Bitmap bitmap(capacity, buffer, sizeof(buffer));

    // 初始状态
    ASSERT_FALSE(bitmap.IsUsed(5));

    // 标记为占用
    bitmap.MarkAsUsed(5);
    EXPECT_TRUE(bitmap.IsUsed(5));
    
    // 标记为空闲
    bitmap.MarkAsFree(5);
    EXPECT_FALSE(bitmap.IsUsed(5));
    
    // 越界检查
    EXPECT_TRUE(bitmap.IsUsed(100));
}

// 测试套件名称: BitmapTest
// 测试用例名称: FindsFirstFreeBlockCorrectly
TEST(BitmapTest, FindsFirstFreeBlockCorrectly) {
    const size_t capacity = 16;
    unsigned char buffer[2];
    Bitmap bitmap(capacity, buffer, sizeof(buffer));

    ASSERT_EQ(bitmap.FindFirstFree(), 0);

    bitmap.MarkAsUsed(0);
    bitmap.MarkAsUsed(1);
    EXPECT_EQ(bitmap.FindFirstFree(), 2);
    
    // 从指定位置开始查找
    bitmap.MarkAsUsed(2);
    EXPECT_EQ(bitmap.FindFirstFree(2), 3);
    
    // 测试位图已满的情况
    for(size_t i = 3; i < capacity; ++i) {
        bitmap.MarkAsUsed(i);
    }
    EXPECT_EQ(bitmap.FindFirstFree(), Bitmap::kNotFound);
}