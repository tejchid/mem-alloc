#include "../include/memalloc/memalloc.h"
#include <gtest/gtest.h>
#include <vector>

TEST(Coalesce, FreeAndReallocLarger) {
    // allocate two large blocks, free both, then allocate something
    // that needs the combined space — only works if coalescing happened
    void* a = ma_malloc(65536);
    void* b = ma_malloc(65536);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ma_free(a);
    ma_free(b);

    // after coalescing, a single 128KB alloc should succeed
    void* c = ma_malloc(131072);
    ASSERT_NE(c, nullptr);
    ma_free(c);
}

TEST(Coalesce, RepeatedChurn) {
    // repeatedly alloc/free large blocks to stress coalescing path
    for (int i = 0; i < 100; i++) {
        void* p = ma_malloc(1024 * 64);
        ASSERT_NE(p, nullptr);
        ma_free(p);
    }
}

TEST(Coalesce, FragmentationMetrics) {
    std::vector<void*> ptrs;
    for (int i = 0; i < 50; i++)
        ptrs.push_back(ma_malloc(4096));

    // free alternating blocks to create external fragmentation
    for (int i = 0; i < 50; i += 2) {
        ma_free(ptrs[i]);
        ptrs[i] = nullptr;
    }

    MA_Stats s;
    ma_stats(&s);
    EXPECT_GT(s.bytes_free, 0u);

    for (int i = 1; i < 50; i += 2)
        ma_free(ptrs[i]);
}