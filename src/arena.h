#pragma once

#include <cstddef>

namespace ma {

void  arena_init();
void* arena_alloc(size_t size);
void  arena_free(void* ptr);

void* arena_alloc_run();
void  arena_free_run(void* run_base);

void  arena_free_stats(size_t* free_bytes_out, size_t* largest_out);

} // namespace ma