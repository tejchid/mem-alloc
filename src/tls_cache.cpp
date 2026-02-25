#include "tls_cache.h"
#include "slab.h"
#include "arena.h"
#include "platform.h"
#include "stats.h"
#include "internal.h"

#include <cstring>

namespace ma {

static thread_local TLSCache* tl_cache = nullptr;

TLSCache* tls_get() {
    if (!tl_cache) {
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

    if (pc.current_run) {
        slab_run_drain_remote(pc.current_run);
        if (pc.current_run->local_free) {
            return slab_run_alloc(pc.current_run);
        }

        if (slab_run_empty(pc.current_run)) {
            arena_free_run(pc.current_run);
            pc.current_run = nullptr;
            pc.run_count--;
        }
    }

    void* mem = arena_alloc_run();
    if (!mem) return nullptr;

    SlabRun* run   = slab_run_init(mem, static_cast<uint32_t>(cls));
    pc.current_run = run;
    pc.run_count++;

    stats_add_metadata(sizeof(SlabRun));
    return slab_run_alloc(run);
}

void* tls_alloc(size_t size) {
    size_t cls = size_class(round8(size));
    TLSCache* cache   = tls_get();
    PerClassCache& pc = cache->classes[cls];

    if (pc.head) {
        void* block = pc.head;
        pc.head = *reinterpret_cast<void**>(block);
        pc.count--;
        stats_slab_inuse_inc();
        return block;
    }

    return refill_from_run(cache, cls);
}

void tls_free(void* ptr, SlabRun* run) {
    TLSCache* cache   = tls_get();
    size_t cls        = run->class_id;
    PerClassCache& pc = cache->classes[cls];

    if (pc.count >= TLS_MAX_LOCAL) {
        slab_run_free(run, ptr);
        return;
    }

    *reinterpret_cast<void**>(ptr) = pc.head;
    pc.head = ptr;
    pc.count++;
    stats_slab_inuse_dec();
}

} // namespace ma