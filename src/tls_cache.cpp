#include "tls_cache.h"
#include "slab.h"
#include "arena.h"
#include "platform.h"
#include "stats.h"

#include <cstring>

namespace ma {

static thread_local TLSCache* tl_cache = nullptr;

TLSCache* tls_get() {
    if (!tl_cache) {
        // allocate TLS cache from OS directly so it doesn't recurse into ma_malloc
        void* mem = platform::vm_alloc(sizeof(TLSCache));
        tl_cache  = static_cast<TLSCache*>(mem);
        for (size_t i = 0; i < SIZE_CLASS_COUNT; i++) {
            tl_cache->classes[i] = {nullptr, 0, 0, nullptr};
        }
        tl_cache->tid = platform::thread_id();
    }
    return tl_cache;
}

static void* refill_from_run(TLSCache* cache, size_t cls) {
    PerClassCache& pc = cache->classes[cls];

    // drain remote frees first if we have a current run
    if (pc.current_run) {
        slab_run_drain_remote(pc.current_run);
        if (pc.current_run->local_free) {
            return slab_run_alloc(pc.current_run);
        }
        // run is full — if empty reclaim it
        if (slab_run_empty(pc.current_run)) {
            arena_free_run(pc.current_run);
            pc.current_run = nullptr;
            pc.run_count--;
        }
    }

    // get a new slab run from arena
    void* mem = arena_alloc_run();
    if (!mem) return nullptr;

    SlabRun* run      = slab_run_init(mem, static_cast<uint32_t>(cls));
    pc.current_run    = run;
    pc.run_count++;
    g_stats.bytes_metadata.fetch_add(sizeof(SlabRun));
    return slab_run_alloc(run);
}

void* tls_alloc(size_t size) {
    size_t cls = size_class(round8(size));
    TLSCache* cache   = tls_get();
    PerClassCache& pc = cache->classes[cls];

    if (pc.head) {
        void* block = pc.head;
        void* next;
        memcpy(&next, block, sizeof(void*));
        pc.head = next;
        pc.count--;
        g_stats.slab_in_use.fetch_add(1);
        return block;
    }

    // local list empty — alloc directly from run
    return refill_from_run(cache, cls);
}

void tls_free(void* ptr, SlabRun* run) {
    TLSCache* cache   = tls_get();
    size_t cls        = run->class_id;
    PerClassCache& pc = cache->classes[cls];

    if (pc.count >= TLS_MAX_LOCAL) {
        // return block directly to its run to bound cache size
        slab_run_free(run, ptr);
        return;
    }

    // push onto local list
    memcpy(ptr, &pc.head, sizeof(void*));
    pc.head = ptr;
    pc.count++;
    g_stats.slab_in_use.fetch_sub(1);
}

} // namespace ma