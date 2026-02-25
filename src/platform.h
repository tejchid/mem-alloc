#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>

namespace ma::platform {

inline void* vm_alloc(size_t size) {
    void* p = ::mmap(nullptr, size,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? nullptr : p;
}

inline void vm_free(void* ptr, size_t size) {
    ::munmap(ptr, size);
}

inline size_t page_size() {
    static size_t ps = static_cast<size_t>(::getpagesize());
    return ps;
}

uint32_t thread_id();

} // namespace ma::platform