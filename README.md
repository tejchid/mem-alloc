# mem-alloc

A concurrent memory allocator written in C++17, built as a drop-in replacement for `malloc`/`free`.

The goal was to understand where `glibc malloc` serializes under concurrent load and build something that avoids the bottleneck for small allocations using thread-local slab caches.

## Architecture

```
malloc(size)
  └─ size <= slab threshold?
       ├─ YES → thread-local slab cache (no lock)
       │         └─ slab empty? → refill from central heap (mmap)
       └─ NO  → large allocation directly via mmap

free(ptr)
  └─ same thread that allocated?
       ├─ YES → return to thread-local slab (no lock)
       └─ NO  → push to lock-free remote_free queue (Treiber stack)
                 └─ owning thread drains on next alloc
```

**Thread-Local Slab Caches** — each thread maintains its own free-list per size class. Small allocations never touch a global lock. 64 size classes cover 8–4096 bytes in geometric steps.

**Boundary-Tag Coalescing** — adjacent free blocks are merged on `free` to reduce fragmentation. Tags stored at block header and footer enable O(1) neighbor lookup.

**Treiber Stack for Cross-Thread Free** — when a pointer is freed by a different thread than the one that allocated it, it is pushed onto the allocating thread's lock-free remote free queue using a Treiber stack with `std::atomic` compare-exchange. The owning thread drains this queue lazily on its next allocation.

**mmap-backed Heap** — memory is requested from the OS via `mmap(MAP_ANONYMOUS)` in large chunks and carved into slabs. This avoids `sbrk` and gives explicit control over virtual address space layout.

## Performance

Benchmarked on MacBook Pro (x86_64, Apple Clang 14, 12 logical cores):

```
BM_MA_Small/threads:1     9.6M ops/sec
BM_MA_Small/threads:4    30.2M ops/sec
BM_MA_Small/threads:8    41.1M ops/sec

BM_MA_CrossThreadFree     1.88M ops/sec  (exercises Treiber stack path)
```

ThreadSanitizer run across all test cases — zero data races detected.

## Build

**Requirements:** macOS with Xcode Command Line Tools, CMake 3.20+, Google Benchmark

```bash
git clone https://github.com/tejchid/mem-alloc
cd mem-alloc
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.logicalcpu)
```

## Run Benchmarks

```bash
cd build
./mem_alloc_bench
```

## Run Tests

```bash
cd build
./mem_alloc_tests
```

To run with ThreadSanitizer:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=thread"
make -j$(sysctl -n hw.logicalcpu)
./mem_alloc_tests
```

To run with AddressSanitizer:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address"
make -j$(sysctl -n hw.logicalcpu)
./mem_alloc_tests
```

## Project Structure

```
mem-alloc/
├── include/memalloc/   # Public API header (ma_malloc, ma_free)
├── src/                # Allocator implementation
├── tests/              # Correctness and stress tests
└── bench/              # Google Benchmark harness
```

## Key Design Decisions

**Why 64 size classes?** Geometric spacing (each class ~1.25x the previous) gives good granularity for small allocations while keeping the slab table small enough to fit in cache.

**Why Treiber stack for remote frees?** A mutex would serialize all cross-thread frees through a single lock. The Treiber stack lets each thread maintain its own queue, drained lazily — no contention on the free path.

**Why mmap instead of sbrk?** `mmap` lets you return memory to the OS independently for each chunk. `sbrk` can only move the program break forward and is not thread-safe.
