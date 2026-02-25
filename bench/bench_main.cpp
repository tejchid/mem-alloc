#include "../include/memalloc/memalloc.h"

#include <benchmark/benchmark.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <thread>

// ── helpers ───────────────────────────────────────────────────────────────────

static void* sys_malloc(size_t s) { return ::malloc(s); }
static void  sys_free(void* p)    { ::free(p); }

using AllocFn = void*(*)(size_t);
using FreeFn  = void(*)(void*);

static void run_alloc_free(benchmark::State& state,
                           AllocFn alloc, FreeFn free_fn,
                           size_t size) {
    for (auto _ : state) {
        void* p = alloc(size);
        benchmark::DoNotOptimize(p);
        free_fn(p);
    }
    state.SetItemsProcessed(state.iterations());
}

// ── small alloc/free ──────────────────────────────────────────────────────────

static void BM_MA_Small(benchmark::State& s)  { run_alloc_free(s, ma_malloc,   ma_free,   64); }
static void BM_SYS_Small(benchmark::State& s) { run_alloc_free(s, sys_malloc, sys_free, 64); }
BENCHMARK(BM_MA_Small)->Threads(1)->Threads(4)->Threads(8);
BENCHMARK(BM_SYS_Small)->Threads(1)->Threads(4)->Threads(8);

// ── large alloc/free ──────────────────────────────────────────────────────────

static void BM_MA_Large(benchmark::State& s)  { run_alloc_free(s, ma_malloc,   ma_free,   65536); }
static void BM_SYS_Large(benchmark::State& s) { run_alloc_free(s, sys_malloc, sys_free, 65536); }
BENCHMARK(BM_MA_Large)->Threads(1)->Threads(4);
BENCHMARK(BM_SYS_Large)->Threads(1)->Threads(4);

// ── churn: alloc many then free ───────────────────────────────────────────────

static void BM_MA_Churn(benchmark::State& s) {
    const int N = 256;
    std::vector<void*> ptrs(N);
    for (auto _ : s) {
        for (int i = 0; i < N; i++) ptrs[i] = ma_malloc(128);
        for (int i = 0; i < N; i++) ma_free(ptrs[i]);
    }
    s.SetItemsProcessed(s.iterations() * N);
}

static void BM_SYS_Churn(benchmark::State& s) {
    const int N = 256;
    std::vector<void*> ptrs(N);
    for (auto _ : s) {
        for (int i = 0; i < N; i++) ptrs[i] = ::malloc(128);
        for (int i = 0; i < N; i++) ::free(ptrs[i]);
    }
    s.SetItemsProcessed(s.iterations() * N);
}
BENCHMARK(BM_MA_Churn)->Threads(1)->Threads(4)->Threads(8);
BENCHMARK(BM_SYS_Churn)->Threads(1)->Threads(4)->Threads(8);

// ── cross-thread free (exercises remote_free path) ────────────────────────────

static void BM_MA_CrossThreadFree(benchmark::State& s) {
    const int N = 64;
    for (auto _ : s) {
        std::vector<void*> ptrs(N);
        for (int i = 0; i < N; i++) ptrs[i] = ma_malloc(64);

        std::thread consumer([&]() {
            for (int i = 0; i < N; i++) ma_free(ptrs[i]);
        });
        consumer.join();
    }
    s.SetItemsProcessed(s.iterations() * N);
}
BENCHMARK(BM_MA_CrossThreadFree);

BENCHMARK_MAIN();