#pragma once

#include "internal.h"

namespace ma {

struct alignas(CACHE_LINE) PerClassCache {
    void*    head;       // local free list
    uint32_t count;      // how many blocks currently cached
    uint32_t run_count;  // how many runs assigned to this thread for this class
    SlabRun* current_run;
};

struct alignas(CACHE_LINE) TLSCache {
    PerClassCache classes[SIZE_CLASS_COUNT];
    uint32_t      tid;
};

TLSCache* tls_get();

void* tls_alloc(size_t size);
void  tls_free(void* ptr, SlabRun* run);

} // namespace ma