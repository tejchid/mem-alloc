#include "../include/memalloc/memalloc.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>

TEST(Threaded, ConcurrentSmallAllocs) {
    const int THREADS = 8;
    const int OPS     = 10000;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; t++) {
        threads.emplace_back([&, t]() {
            std::vector<void*> ptrs;
            for (int i = 0; i < OPS; i++) {
                void* p = ma_malloc(64);
                if (!p) { errors++; continue; }
                memset(p, t, 64);
                ptrs.push_back(p);
            }
            for (void* p : ptrs) {
                uint8_t* b = static_cast<uint8_t*>(p);
                for (int j = 0; j < 64; j++) {
                    if (b[j] != (uint8_t)t) { errors++; break; }
                }
                ma_free(p);
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(errors.load(), 0);
}

TEST(Threaded, CrossThreadFree) {
    // one thread allocates, another frees — exercises remote_free path
    const int N = 1000;
    std::vector<void*> ptrs(N);

    std::thread producer([&]() {
        for (int i = 0; i < N; i++)
            ptrs[i] = ma_malloc(128);
    });
    producer.join();

    std::thread consumer([&]() {
        for (int i = 0; i < N; i++)
            ma_free(ptrs[i]);
    });
    consumer.join();
}

TEST(Threaded, MixedSizes) {
    const int THREADS = 4;
    std::vector<std::thread> threads;

    for (int t = 0; t < THREADS; t++) {
        threads.emplace_back([t]() {
            size_t sizes[] = {8, 64, 128, 256, 512, 1024, 8192};
            for (int i = 0; i < 1000; i++) {
                size_t sz = sizes[i % 7];
                void* p = ma_malloc(sz);
                if (p) {
                    memset(p, t, sz > 64 ? 64 : sz);
                    ma_free(p);
                }
            }
        });
    }
    for (auto& th : threads) th.join();
}