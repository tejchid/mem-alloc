#pragma once

#include <cstddef>
#include <atomic>

namespace ma {

struct Stats {
    std::atomic<size_t> bytes_requested{0};
    std::atomic<size_t> bytes_allocated{0};
    std::atomic<size_t> bytes_metadata{0};
    std::atomic<size_t> slab_in_use{0};
    std::atomic<size_t> slab_capacity{0};
};

extern Stats g_stats;

} // namespace ma