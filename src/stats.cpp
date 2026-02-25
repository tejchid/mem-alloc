#include "stats.h"
#include "arena.h"
#include "../include/memalloc/memalloc.h"

#include <cstdio>

namespace ma {

Stats g_stats;

} // namespace ma

extern "C" void ma_stats(MA_Stats* out) {
    using namespace ma;
    out->bytes_requested    = g_stats.bytes_requested.load();
    out->bytes_allocated    = g_stats.bytes_allocated.load();
    out->bytes_metadata     = g_stats.bytes_metadata.load();
    out->slab_in_use        = g_stats.slab_in_use.load();
    out->slab_capacity      = g_stats.slab_capacity.load();

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
        ? 1.0 - (double)s.bytes_requested / s.bytes_allocated : 0.0;
    double ext_frag = s.bytes_free > 0
        ? 1.0 - (double)s.largest_free_block / s.bytes_free : 0.0;
    printf("  internal frag:  %.1f%%\n", int_frag * 100.0);
    printf("  external frag:  %.1f%%\n", ext_frag * 100.0);
}