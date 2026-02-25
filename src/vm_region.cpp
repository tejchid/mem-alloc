#include "platform.h"
#include <cstdint>
#include <atomic>

namespace ma::platform {

static std::atomic<uint32_t> g_tid_counter{0};
static thread_local uint32_t tl_tid = UINT32_MAX;

uint32_t thread_id() {
    if (tl_tid == UINT32_MAX) {
        tl_tid = g_tid_counter.fetch_add(1, std::memory_order_relaxed);
    }
    return tl_tid;
}

} // namespace ma::platform