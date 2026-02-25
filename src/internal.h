#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <atomic>

namespace ma {

static constexpr size_t   CACHE_LINE        = 64;
static constexpr size_t   SMALL_MAX         = 512;
static constexpr size_t   SIZE_CLASS_COUNT  = 64;
static constexpr size_t   RUN_SIZE          = 65536;
static constexpr size_t   ARENA_REGION_SIZE = 67108864;
static constexpr size_t   TLS_MAX_LOCAL     = 256;
static constexpr uint64_t BLOCK_MAGIC       = 0xDEADC0DEDEADC0DEULL;
static constexpr uint32_t RUN_MAGIC         = 0xA110CA7E;

// size class index for sizes 8..512 in steps of 8
inline size_t size_class(size_t size) {
    return (size + 7) / 8 - 1;
}

inline size_t class_to_size(size_t cls) {
    return (cls + 1) * 8;
}

// round up to next multiple of 8
inline size_t round8(size_t n) {
    return (n + 7) & ~size_t(7);
}

inline bool is_power_of_two(size_t n) {
    return n && !(n & (n - 1));
}

// ── Global block header/footer (boundary tags) ────────────────────────────────
// used by the arena for large allocations (>512B)
// header sits just before user payload
// footer sits at end of block — enables O(1) coalescing with prev block

struct BlockHeader {
    size_t   size;       // includes header + footer, always multiple of 8
    bool     in_use;
    bool     is_slab;    // true if this block is backing a slab run
    uint64_t magic;
};

struct BlockFooter {
    size_t size;         // must match header size
};

static constexpr size_t BLOCK_HEADER_SIZE = sizeof(BlockHeader);
static constexpr size_t BLOCK_FOOTER_SIZE = sizeof(BlockFooter);
static constexpr size_t BLOCK_OVERHEAD    = BLOCK_HEADER_SIZE + BLOCK_FOOTER_SIZE;
static constexpr size_t MIN_BLOCK_SIZE    = BLOCK_OVERHEAD + 8;

inline BlockHeader* payload_to_header(void* payload) {
    return reinterpret_cast<BlockHeader*>(
        static_cast<char*>(payload) - BLOCK_HEADER_SIZE);
}

inline void* header_to_payload(BlockHeader* h) {
    return reinterpret_cast<char*>(h) + BLOCK_HEADER_SIZE;
}

inline BlockFooter* header_to_footer(BlockHeader* h) {
    return reinterpret_cast<BlockFooter*>(
        reinterpret_cast<char*>(h) + h->size - BLOCK_FOOTER_SIZE);
}

inline BlockHeader* footer_to_header(BlockFooter* f) {
    return reinterpret_cast<BlockHeader*>(
        reinterpret_cast<char*>(f) + BLOCK_FOOTER_SIZE - f->size);
}

// ── Slab run header ───────────────────────────────────────────────────────────
// sits at start of a RUN_SIZE-aligned block
// owner thread uses local_free; other threads push to remote_free

struct alignas(CACHE_LINE) SlabRun {
    uint32_t              magic;
    uint32_t              class_id;
    uint32_t              block_size;
    uint32_t              capacity;
    uint32_t              in_use;
    uint32_t              owner_tid;
    SlabRun*              next_run;
    void*                 local_free;   // intrusive free list for owner thread

    // remote_free on its own cache line so remote writers don't false-share
    // with owner's hot fields above
    alignas(CACHE_LINE) std::atomic<void*> remote_free;

    // blocks start here after header rounded to CACHE_LINE
    char data[];
};

static_assert(sizeof(SlabRun) <= 2 * CACHE_LINE,
              "SlabRun header too large");

} // namespace ma