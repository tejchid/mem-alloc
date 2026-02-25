cmake_minimum_required(VERSION 3.20)
project(mem_alloc CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_ASAN "AddressSanitizer + UBSan" OFF)
option(ENABLE_TSAN "ThreadSanitizer"          OFF)

add_compile_options(-O2 -Wall -Wextra)
add_compile_definitions(MA_ENABLE_STATS=0)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

find_package(Threads REQUIRED)

add_library(memalloc STATIC
    src/vm_region.cpp
    src/arena.cpp
    src/slab.cpp
    src/tls_cache.cpp
    src/stats.cpp
    src/api.cpp
)

target_include_directories(memalloc
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(memalloc PUBLIC Threads::Threads)

find_package(GTest QUIET)
if(GTest_FOUND)
    enable_testing()
    add_executable(test_memalloc
        tests/test_basic.cpp
        tests/test_coalesce.cpp
        tests/test_threaded.cpp
    )
    target_link_libraries(test_memalloc PRIVATE memalloc GTest::gtest GTest::gtest_main)
    add_test(NAME memalloc_tests COMMAND test_memalloc)
endif()

find_package(benchmark QUIET)
if(benchmark_FOUND)
    add_executable(bench_memalloc bench/bench_main.cpp)
    target_link_libraries(bench_memalloc PRIVATE memalloc benchmark::benchmark)
endif()