#include "arena.h"
#include "internal.h"
#include "platform.h"
#include "stats.h"

#include <cstring>
#include <mutex>
#include <cassert>

namespace ma {

struct FreeNode {
    FreeNode* prev;
    FreeNode* next;
};

struct ArenaRegion {
    char*         start;
    char*         end;
    ArenaRegion*  next;
};

static ArenaRegion* g_regions   = nullptr;
static FreeNode*    g_free_list = nullptr;
static std::mutex   g_arena_lock;

static ArenaRegion* new_region(size_t min_size) {
    size_t sz = ARENA_REGION_SIZE;
    while (sz < min_size + BLOCK_OVERHEAD + sizeof(ArenaRegion))
        sz *= 2;

    char* mem = static_cast<char*>(platform::vm_alloc(sz));
    if (!mem) return nullptr;

    ArenaRegion* r = reinterpret_cast<ArenaRegion*>(mem);
    r->start = mem + sizeof(ArenaRegion);
    r->end   = mem + sz;
    r->next  = nullptr;

    size_t block_sz = r->end - r->start;

    BlockHeader* h = reinterpret_cast<BlockHeader*>(r->start);
    h->size    = block_sz;
    h->in_use  = false;
    h->is_slab = false;
    h->magic   = BLOCK_MAGIC;

    header_to_footer(h)->size = block_sz;

    FreeNode* node = reinterpret_cast<FreeNode*>(header_to_payload(h));
    node->prev = nullptr;
    node->next = g_free_list;
    if (g_free_list) g_free_list->prev = node;
    g_free_list = node;

    return r;
}

static ArenaRegion* region_of(void* ptr) {
    char* p = static_cast<char*>(ptr);
    for (ArenaRegion* r = g_regions; r; r = r->next) {
        if (p >= r->start && p < r->end)
            return r;
    }
    return nullptr;
}

static void free_list_remove(FreeNode* node) {
    if (node->prev) node->prev->next = node->next;
    else            g_free_list      = node->next;
    if (node->next) node->next->prev = node->prev;
}

static void free_list_insert(BlockHeader* h) {
    FreeNode* node = reinterpret_cast<FreeNode*>(header_to_payload(h));
    node->prev = nullptr;
    node->next = g_free_list;
    if (g_free_list) g_free_list->prev = node;
    g_free_list = node;
}

void arena_init() {
    std::lock_guard<std::mutex> lock(g_arena_lock);
    if (!g_regions)
        g_regions = new_region(ARENA_REGION_SIZE);
}

void* arena_alloc(size_t size) {
    size = round8(size);
    size_t needed = size + BLOCK_OVERHEAD;
    if (needed < MIN_BLOCK_SIZE)
        needed = MIN_BLOCK_SIZE;

    std::lock_guard<std::mutex> lock(g_arena_lock);

    for (;;) {
        for (FreeNode* node = g_free_list; node; node = node->next) {
            BlockHeader* h = payload_to_header(node);

            if (h->size >= needed) {
                free_list_remove(node);

                if (h->size >= needed + MIN_BLOCK_SIZE) {
                    size_t rem_size = h->size - needed;

                    h->size = needed;
                    header_to_footer(h)->size = needed;

                    BlockHeader* rem =
                        reinterpret_cast<BlockHeader*>(
                            reinterpret_cast<char*>(h) + needed);

                    rem->size    = rem_size;
                    rem->in_use  = false;
                    rem->is_slab = false;
                    rem->magic   = BLOCK_MAGIC;
                    header_to_footer(rem)->size = rem_size;

                    free_list_insert(rem);
                }

                h->in_use = true;
                return header_to_payload(h);
            }
        }

        ArenaRegion* r = new_region(needed);
        if (!r) return nullptr;

        r->next = g_regions;
        g_regions = r;
    }
}

void arena_free(void* ptr) {
    if (!ptr) return;

    BlockHeader* h = payload_to_header(ptr);
    std::lock_guard<std::mutex> lock(g_arena_lock);

    h->in_use = false;

    ArenaRegion* region = region_of(h);
    if (!region) return;

    char* next_addr = reinterpret_cast<char*>(h) + h->size;

    if (next_addr < region->end) {
        BlockHeader* next = reinterpret_cast<BlockHeader*>(next_addr);
        if (!next->in_use && next->magic == BLOCK_MAGIC) {
            free_list_remove(
                reinterpret_cast<FreeNode*>(header_to_payload(next)));

            h->size += next->size;
            header_to_footer(h)->size = h->size;
        }
    }

    if (reinterpret_cast<char*>(h) > region->start) {
        BlockFooter* prev_footer =
            reinterpret_cast<BlockFooter*>(
                reinterpret_cast<char*>(h) - BLOCK_FOOTER_SIZE);

        BlockHeader* prev = footer_to_header(prev_footer);
        if (!prev->in_use && prev->magic == BLOCK_MAGIC) {
            free_list_remove(
                reinterpret_cast<FreeNode*>(header_to_payload(prev)));

            prev->size += h->size;
            header_to_footer(prev)->size = prev->size;
            h = prev;
        }
    }

    free_list_insert(h);
}

void* arena_alloc_run() {
    return platform::vm_alloc(RUN_SIZE);
}

void arena_free_run(void* run_base) {
    platform::vm_free(run_base, RUN_SIZE);
}

void arena_free_stats(size_t* free_bytes_out, size_t* largest_out) {
    std::lock_guard<std::mutex> lock(g_arena_lock);

    size_t total = 0, largest = 0;

    for (FreeNode* node = g_free_list; node; node = node->next) {
        BlockHeader* h = payload_to_header(node);
        size_t payload_sz = h->size - BLOCK_OVERHEAD;

        total += payload_sz;
        if (payload_sz > largest)
            largest = payload_sz;
    }

    *free_bytes_out = total;
    *largest_out    = largest;
}

} // namespace ma