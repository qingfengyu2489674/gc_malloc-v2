// SizeClassConfig_test.cpp
#include <gtest/gtest.h>
#include "gc_malloc/ThreadHeap/SizeClassConfig.hpp"

using gc::SizeClassConfig;

TEST(SizeClassConfig, MinAndMaxBoundaries) {
    // 最小边界：<= kMinAlloc 应映射到第0个size-class，Normalize 后得到 kMinAlloc
    EXPECT_EQ(SizeClassConfig::SizeToClass(0), 0u);
    EXPECT_EQ(SizeClassConfig::SizeToClass(1), 0u);
    EXPECT_EQ(SizeClassConfig::SizeToClass(SizeClassConfig::kMinAlloc), 0u);
    EXPECT_EQ(SizeClassConfig::Normalize(0), SizeClassConfig::kMinAlloc);
    EXPECT_EQ(SizeClassConfig::Normalize(1), SizeClassConfig::kMinAlloc);
    EXPECT_EQ(SizeClassConfig::Normalize(SizeClassConfig::kMinAlloc), SizeClassConfig::kMinAlloc);

    // 大于小对象上限：应映射到最后一个class（通常上层会走大对象路径）
    const std::size_t last_idx = SizeClassConfig::ClassCount() - 1;
    EXPECT_EQ(SizeClassConfig::SizeToClass(SizeClassConfig::kMaxSmallAlloc + 1), last_idx);
    EXPECT_EQ(SizeClassConfig::ClassToSize(last_idx), SizeClassConfig::kMaxSmallAlloc);
    EXPECT_EQ(SizeClassConfig::Normalize(SizeClassConfig::kMaxSmallAlloc + 1),
              SizeClassConfig::kMaxSmallAlloc);
}

TEST(SizeClassConfig, AlignmentAndMonotonicity) {
    // 所有 class 的块大小应 >= kMinAlloc 且按 kAlignment 对齐，且单调非降
    const std::size_t n = SizeClassConfig::ClassCount();
    ASSERT_GT(n, 0u);

    std::size_t prev = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t sz = SizeClassConfig::ClassToSize(i);
        EXPECT_GE(sz, SizeClassConfig::kMinAlloc) << "i=" << i;
        EXPECT_EQ(sz % SizeClassConfig::kAlignment, 0u) << "i=" << i << " sz=" << sz;
        if (i > 0) {
            EXPECT_GE(sz, prev) << "size table must be non-decreasing";
        }
        prev = sz;
    }
    // 最后一个应为 kMaxSmallAlloc
    EXPECT_EQ(SizeClassConfig::ClassToSize(n - 1), SizeClassConfig::kMaxSmallAlloc);
}

TEST(SizeClassConfig, NormalizeRoundsUp) {
    // Normalize 对典型值的“向上取整”行为
    const std::size_t minv = SizeClassConfig::kMinAlloc;

    // 比如：恰好是最小值
    EXPECT_EQ(SizeClassConfig::Normalize(minv), minv);

    // 介于两个class之间：应向上取整到下一个class的大小
    // 这里构造两个相邻 class 的样本进行验证
    const std::size_t n = SizeClassConfig::ClassCount();
    ASSERT_GE(n, 3u); // 确保至少有几个class

    for (std::size_t i = 0; i + 1 < n; ++i) {
        const std::size_t a = SizeClassConfig::ClassToSize(i);
        const std::size_t b = SizeClassConfig::ClassToSize(i + 1);
        // 在 (a, b] 范围内的任一请求都应 Normalize 到 b
        if (b > a) {
            const std::size_t mid = a + (b - a) / 2;  // a < mid < b（若步长>1）
            if (mid > a) {
                EXPECT_EQ(SizeClassConfig::Normalize(mid), b) << "between " << a << " and " << b;
            }
            EXPECT_EQ(SizeClassConfig::Normalize(b), b);
        } else {
            // 允许相等（非降）；相等时 normalize(mid) 应等于 b==a
            EXPECT_EQ(SizeClassConfig::Normalize(a), a);
        }
    }
}

TEST(SizeClassConfig, SizeToClassAndBackIsConsistent) {
    // 对一系列请求字节数，验证 SizeToClass ↔ ClassToSize/Normalize 的一致性
    // 取多个覆盖点：小到大、跨越多段增长区间
    const std::size_t probes[] = {
        1, 16, 31, 32, 33, 47, 48, 63, 64, 80, 96, 127, 128,
        160, 192, 224, 256, 300, 512, 800, 1024, 1500, 2048,
        4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288,
        SizeClassConfig::kMaxSmallAlloc - 1, SizeClassConfig::kMaxSmallAlloc,
        SizeClassConfig::kMaxSmallAlloc + 1
    };

    for (std::size_t nbytes : probes) {
        const std::size_t idx = SizeClassConfig::SizeToClass(nbytes);
        ASSERT_LT(idx, SizeClassConfig::ClassCount());
        const std::size_t sz  = SizeClassConfig::ClassToSize(idx);

        // Normalize 应等于 ClassToSize(SizeToClass())
        EXPECT_EQ(SizeClassConfig::Normalize(nbytes), sz) << "nbytes=" << nbytes;

        // sz 必须 >= nbytes（向上取整），除非 nbytes > kMaxSmallAlloc 时回落到最后一个
        if (nbytes <= SizeClassConfig::kMaxSmallAlloc) {
            EXPECT_GE(sz, nbytes) << "nbytes=" << nbytes;
        } else {
            EXPECT_EQ(sz, SizeClassConfig::kMaxSmallAlloc);
        }

        // 对齐性
        EXPECT_EQ(sz % SizeClassConfig::kAlignment, 0u);
        EXPECT_GE(sz, SizeClassConfig::kMinAlloc);
    }
}

TEST(SizeClassConfig, FirstFewMappingsSanity) {
    // spot-check：前几个映射是否符合直觉（32、33、48 等）
    const auto c0 = SizeClassConfig::SizeToClass(32);
    EXPECT_EQ(SizeClassConfig::ClassToSize(c0), 32u);

    const auto c1 = SizeClassConfig::SizeToClass(33);
    EXPECT_GE(SizeClassConfig::ClassToSize(c1), 33u);

    const auto c2 = SizeClassConfig::SizeToClass(48);
    EXPECT_EQ(SizeClassConfig::ClassToSize(c2), SizeClassConfig::Normalize(48));

    const auto c3 = SizeClassConfig::SizeToClass(49);
    EXPECT_EQ(SizeClassConfig::ClassToSize(c3), SizeClassConfig::Normalize(49));
}
