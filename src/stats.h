#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>

namespace ma {

// Global counters (still atomics, but we hit them rarely via batching)
struct Stats {
    std::atomic<size_t> bytes_requested{0};
    std::atomic<size_t> bytes_allocated{0};
    std::atomic<size_t> bytes_metadata{0};
    std::atomic<size_t> slab_in_use{0};
    std::atomic<size_t> slab_capacity{0};
};

extern Stats g_stats;

// ----- Batched hot-path API -----
// Goal: avoid atomics on every alloc/free.
// We batch deltas in TLS and flush occasionally (relaxed atomics).

void stats_add_requested(size_t bytes);
void stats_add_allocated(size_t bytes);
void stats_sub_allocated(size_t bytes);

void stats_add_metadata(size_t bytes);

void stats_slab_inuse_inc();
void stats_slab_inuse_dec();
void stats_slab_capacity_add(size_t blocks);

} // namespace ma