cmake_minimum_required(VERSION 3.16)
project(concurrent_memory_pool CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

if(MSVC)
    add_compile_options(/O2 /W4)
else()
    add_compile_options(-O3 -Wall -Wextra -pthread)
endif()

add_executable(test_allocator tests/test_allocator.cpp)
add_executable(benchmark_allocator benchmarks/benchmark_allocator.cpp)

if(NOT MSVC)
    target_link_libraries(test_allocator PRIVATE pthread)
    target_link_libraries(benchmark_allocator PRIVATE pthread)
endif()
