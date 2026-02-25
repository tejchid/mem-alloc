#include "slab.h"
#include "platform.h"
#include "stats.h"

#include <cstring>

namespace ma {

SlabRun* slab_run_init(void* mem, uint32_t class_id) {
    SlabRun* run       = static_cast<SlabRun*>(mem);
    run->magic         = RUN_MAGIC;
    run->class_id      = class_id;
    run->block_size    = static_cast<uint32_t>(class_to_size(class_id));
    run->owner_tid     = platform::thread_id();
    run->next_run      = nullptr;
    run->local_free    = nullptr;
    run->remote_free.store(nullptr, std::memory_order_relaxed);

    // blocks start after header, aligned to CACHE_LINE
    size_t header_sz = (sizeof(SlabRun) + CACHE_LINE - 1) & ~(CACHE_LINE - 1);
    char*  base      = static_cast<char*>(mem) + header_sz;
    size_t usable    = RUN_SIZE - header_sz;
    run->capacity    = static_cast<uint32_t>(usable / run->block_size);
    run->in_use      = 0;

    // build intrusive free list through all blocks
    // each free block stores a next pointer in its first bytes
    for (uint32_t i = 0; i < run->capacity; i++) {
        void* block = base + i * run->block_size;
        void* next  = (i + 1 < run->capacity)
                      ? base + (i + 1) * run->block_size
                      : nullptr;
        memcpy(block, &next, sizeof(void*));
    }
    run->local_free = base; // head of free list

    g_stats.slab_capacity.fetch_add(run->capacity);
    return run;
}

void* slab_run_alloc(SlabRun* run) {
    if (!run->local_free) return nullptr;

    void* block = run->local_free;
    void* next;
    memcpy(&next, block, sizeof(void*));
    run->local_free = next;
    run->in_use++;
    g_stats.slab_in_use.fetch_add(1);
    return block;
}

void slab_run_free(SlabRun* run, void* ptr) {
    if (platform::thread_id() == run->owner_tid) {
        // owner thread — push to local free list directly
        memcpy(ptr, &run->local_free, sizeof(void*));
        run->local_free = ptr;
        run->in_use--;
        g_stats.slab_in_use.fetch_sub(1);
    } else {
        // remote thread — atomic push onto remote_free Treiber stack
        // acquire-release so the block contents are visible to owner
        void* old_head = run->remote_free.load(std::memory_order_relaxed);
        do {
            memcpy(ptr, &old_head, sizeof(void*));
        } while (!run->remote_free.compare_exchange_weak(
            old_head, ptr,
            std::memory_order_release,
            std::memory_order_relaxed));
        // in_use decremented by owner when it drains
    }
}

void slab_run_drain_remote(SlabRun* run) {
    // owner thread calls this to absorb remotely freed blocks
    void* head = run->remote_free.exchange(nullptr, std::memory_order_acquire);
    while (head) {
        void* next;
        memcpy(&next, head, sizeof(void*));
        memcpy(head, &run->local_free, sizeof(void*));
        run->local_free = head;
        run->in_use--;
        g_stats.slab_in_use.fetch_sub(1);
        head = next;
    }
}

bool slab_run_empty(SlabRun* run) {
    return run->in_use == 0;
}

SlabRun* slab_run_of(void* ptr) {
    // runs are RUN_SIZE-aligned so masking gives the run base
    uintptr_t base = reinterpret_cast<uintptr_t>(ptr) & ~(RUN_SIZE - 1);
    return reinterpret_cast<SlabRun*>(base);
}

} // namespace ma