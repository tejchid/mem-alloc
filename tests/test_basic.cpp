#include "../include/memalloc/memalloc.h"
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

TEST(Basic, MallocFree) {
    void* p = ma_malloc(64);
    ASSERT_NE(p, nullptr);
    memset(p, 0xAB, 64);
    ma_free(p);
}

TEST(Basic, CallocZeroed) {
    int* p = static_cast<int*>(ma_calloc(16, sizeof(int)));
    ASSERT_NE(p, nullptr);
    for (int i = 0; i < 16; i++) EXPECT_EQ(p[i], 0);
    ma_free(p);
}

TEST(Basic, Realloc) {
    void* p = ma_malloc(32);
    ASSERT_NE(p, nullptr);
    memset(p, 0xCC, 32);
    void* p2 = ma_realloc(p, 128);
    ASSERT_NE(p2, nullptr);
    uint8_t* b = static_cast<uint8_t*>(p2);
    for (int i = 0; i < 32; i++) EXPECT_EQ(b[i], 0xCC);
    ma_free(p2);
}

TEST(Basic, NullFree) {
    ma_free(nullptr);
}

TEST(Basic, ZeroMalloc) {
    void* p = ma_malloc(0);
    EXPECT_EQ(p, nullptr);
}

TEST(Basic, AllSizeClasses) {
    for (size_t s = 8; s <= 512; s += 8) {
        void* p = ma_malloc(s);
        ASSERT_NE(p, nullptr);
        memset(p, 0x42, s);
        ma_free(p);
    }
}

TEST(Basic, LargeAlloc) {
    void* p = ma_malloc(1024 * 1024);
    ASSERT_NE(p, nullptr);
    memset(p, 0, 1024 * 1024);
    ma_free(p);
}

TEST(Basic, ManySmallAllocs) {
    std::vector<void*> ptrs;
    for (int i = 0; i < 10000; i++) {
        void* p = ma_malloc(64);
        ASSERT_NE(p, nullptr);
        memset(p, i & 0xFF, 64);
        ptrs.push_back(p);
    }
    for (void* p : ptrs) ma_free(p);
}