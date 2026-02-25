#include "../include/memalloc/memalloc.h"

#include "internal.h"
#include "arena.h"
#include "slab.h"
#include "tls_cache.h"
#include "stats.h"

#include <cstdint>   // uintptr_t
#include <cstring>
#include <mutex>     // std::once_flag, std::call_once

namespace ma {

static std::once_flag g_init_flag;

static void init() {
    arena_init();
}

} // namespace ma

extern "C" void* ma_malloc(size_t size) {
    if (size == 0) return nullptr;
    std::call_once(ma::g_init_flag, ma::init);

    ma::g_stats.bytes_requested.fetch_add(size);

    if (size <= ma::SMALL_MAX) {
        void* ptr = ma::tls_alloc(size);
        if (ptr) {
            ma::g_stats.bytes_allocated.fetch_add(
                ma::class_to_size(ma::size_class(ma::round8(size))));
        }
        return ptr;
    }

    return ma::arena_alloc(size);
}

extern "C" void ma_free(void* ptr) {
    if (!ptr) return;

    uintptr_t base = reinterpret_cast<uintptr_t>(ptr) & ~(ma::RUN_SIZE - 1);
    ma::SlabRun* run = reinterpret_cast<ma::SlabRun*>(base);

    if (run->magic == ma::RUN_MAGIC) {
        ma::g_stats.bytes_allocated.fetch_sub(
            ma::class_to_size(run->class_id));
        ma::tls_free(ptr, run);
        return;
    }

    ma::BlockHeader* h = ma::payload_to_header(ptr);
    if (h->magic == ma::BLOCK_MAGIC) {
        ma::arena_free(ptr);
    }
}

extern "C" void* ma_calloc(size_t count, size_t size) {
    size_t total = count * size;
    void* ptr = ma_malloc(total);
    if (ptr) std::memset(ptr, 0, total);
    return ptr;
}

extern "C" void* ma_realloc(void* ptr, size_t new_size) {
    if (!ptr) return ma_malloc(new_size);
    if (!new_size) { ma_free(ptr); return nullptr; }

    uintptr_t base = reinterpret_cast<uintptr_t>(ptr) & ~(ma::RUN_SIZE - 1);
    ma::SlabRun* run = reinterpret_cast<ma::SlabRun*>(base);

    size_t old_size;
    if (run->magic == ma::RUN_MAGIC) {
        old_size = ma::class_to_size(run->class_id);

        if (new_size <= ma::SMALL_MAX &&
            ma::size_class(ma::round8(new_size)) == run->class_id) {
            return ptr;
        }
    } else {
        ma::BlockHeader* h = ma::payload_to_header(ptr);
        old_size = h->size - ma::BLOCK_OVERHEAD;
    }

    void* new_ptr = ma_malloc(new_size);
    if (!new_ptr) return nullptr;

    std::memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
    ma_free(ptr);
    return new_ptr;
}