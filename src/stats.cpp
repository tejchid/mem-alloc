#include "stats.h"
#include "arena.h"
#include "../include/memalloc/memalloc.h"

#include <cstdio>
#include <algorithm>

namespace ma {

Stats g_stats;

// Tune flush thresholds (bigger = fewer atomics)
static constexpr size_t FLUSH_BYTES_THRESHOLD = 64 * 1024; // 64 KiB
static constexpr size_t FLUSH_OPS_THRESHOLD   = 4096;

struct TLSBatchedStats {
    size_t req_bytes = 0;
    size_t alloc_bytes_add = 0;
    size_t alloc_bytes_sub = 0;
    size_t meta_bytes = 0;

    size_t slab_inuse_inc = 0;
    size_t slab_inuse_dec = 0;
    size_t slab_capacity_add = 0;

    size_t ops = 0;
};

static thread_local TLSBatchedStats tl;

static inline void flush_if_needed() {
    if (tl.ops < FLUSH_OPS_THRESHOLD &&
        (tl.req_bytes + tl.alloc_bytes_add + tl.alloc_bytes_sub + tl.meta_bytes) < FLUSH_BYTES_THRESHOLD) {
        return;
    }

    if (tl.req_bytes) {
        g_stats.bytes_requested.fetch_add(tl.req_bytes, std::memory_order_relaxed);
        tl.req_bytes = 0;
    }
    if (tl.alloc_bytes_add) {
        g_stats.bytes_allocated.fetch_add(tl.alloc_bytes_add, std::memory_order_relaxed);
        tl.alloc_bytes_add = 0;
    }
    if (tl.alloc_bytes_sub) {
        g_stats.bytes_allocated.fetch_sub(tl.alloc_bytes_sub, std::memory_order_relaxed);
        tl.alloc_bytes_sub = 0;
    }
    if (tl.meta_bytes) {
        g_stats.bytes_metadata.fetch_add(tl.meta_bytes, std::memory_order_relaxed);
        tl.meta_bytes = 0;
    }

    if (tl.slab_inuse_inc) {
        g_stats.slab_in_use.fetch_add(tl.slab_inuse_inc, std::memory_order_relaxed);
        tl.slab_inuse_inc = 0;
    }
    if (tl.slab_inuse_dec) {
        g_stats.slab_in_use.fetch_sub(tl.slab_inuse_dec, std::memory_order_relaxed);
        tl.slab_inuse_dec = 0;
    }
    if (tl.slab_capacity_add) {
        g_stats.slab_capacity.fetch_add(tl.slab_capacity_add, std::memory_order_relaxed);
        tl.slab_capacity_add = 0;
    }

    tl.ops = 0;
}

void stats_add_requested(size_t bytes) {
    tl.req_bytes += bytes;
    tl.ops++;
    flush_if_needed();
}
void stats_add_allocated(size_t bytes) {
    tl.alloc_bytes_add += bytes;
    tl.ops++;
    flush_if_needed();
}
void stats_sub_allocated(size_t bytes) {
    tl.alloc_bytes_sub += bytes;
    tl.ops++;
    flush_if_needed();
}
void stats_add_metadata(size_t bytes) {
    tl.meta_bytes += bytes;
    tl.ops++;
    flush_if_needed();
}
void stats_slab_inuse_inc() {
    tl.slab_inuse_inc += 1;
    tl.ops++;
    flush_if_needed();
}
void stats_slab_inuse_dec() {
    tl.slab_inuse_dec += 1;
    tl.ops++;
    flush_if_needed();
}
void stats_slab_capacity_add(size_t blocks) {
    tl.slab_capacity_add += blocks;
    tl.ops++;
    flush_if_needed();
}

} // namespace ma

extern "C" void ma_stats(MA_Stats* out) {
    using namespace ma;

    // NOTE: we intentionally do NOT try to flush other threads' TLS batches.
    // These are approximate counters by design — fast, not perfectly synchronized.
    out->bytes_requested    = g_stats.bytes_requested.load(std::memory_order_relaxed);
    out->bytes_allocated    = g_stats.bytes_allocated.load(std::memory_order_relaxed);
    out->bytes_metadata     = g_stats.bytes_metadata.load(std::memory_order_relaxed);
    out->slab_in_use        = g_stats.slab_in_use.load(std::memory_order_relaxed);
    out->slab_capacity      = g_stats.slab_capacity.load(std::memory_order_relaxed);

    size_t free_bytes = 0, largest = 0;
    ma::arena_free_stats(&free_bytes, &largest);
    out->bytes_free          = free_bytes;
    out->largest_free_block  = largest;
}

extern "C" void ma_print_stats(void) {
    MA_Stats s;
    ma_stats(&s);

    printf("=== mem-alloc stats ===\n");
    printf("  requested:      %zu B\n",  s.bytes_requested);
    printf("  allocated:      %zu B\n",  s.bytes_allocated);
    printf("  metadata:       %zu B\n",  s.bytes_metadata);
    printf("  free (arena):   %zu B\n",  s.bytes_free);
    printf("  largest free:   %zu B\n",  s.largest_free_block);
    printf("  slab in use:    %zu\n",    s.slab_in_use);
    printf("  slab capacity:  %zu\n",    s.slab_capacity);

    double int_frag = s.bytes_allocated > 0
        ? 1.0 - (double)s.bytes_requested / (double)s.bytes_allocated : 0.0;
    double ext_frag = s.bytes_free > 0
        ? 1.0 - (double)s.largest_free_block / (double)s.bytes_free : 0.0;

    printf("  internal frag:  %.1f%%\n", int_frag * 100.0);
    printf("  external frag:  %.1f%%\n", ext_frag * 100.0);
}