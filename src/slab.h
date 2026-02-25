#pragma once

#include "internal.h"

namespace ma {

// initialize a freshly mmap'd RUN_SIZE block as a slab run for class_id
SlabRun* slab_run_init(void* mem, uint32_t class_id);

// allocate one block from a run — caller must be owner thread
void* slab_run_alloc(SlabRun* run);

// free a block back to its run — detects owner vs remote thread
void slab_run_free(SlabRun* run, void* ptr);

// drain remote_free stack into local_free — call before alloc when local empty
void slab_run_drain_remote(SlabRun* run);

// true if run has no live allocations
bool slab_run_empty(SlabRun* run);

// given any pointer allocated from slab tier, find its run header
SlabRun* slab_run_of(void* ptr);

} // namespace ma