#pragma once

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

void* ma_malloc(size_t size);
void  ma_free(void* ptr);
void* ma_realloc(void* ptr, size_t new_size);
void* ma_calloc(size_t count, size_t size);

typedef struct {
    size_t bytes_requested;
    size_t bytes_allocated;
    size_t bytes_metadata;
    size_t bytes_free;
    size_t largest_free_block;
    size_t slab_in_use;
    size_t slab_capacity;
} MA_Stats;

void ma_stats(MA_Stats* out);
void ma_print_stats(void);

#ifdef __cplusplus
}
#endif