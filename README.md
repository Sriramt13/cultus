# Concurrent Memory Pool Allocator

Author: Sriram Teja
Language: C++20

## Summary
A high-throughput, thread-safe memory pool allocator in C++20 using a lock-free Treiber stack with tagged CAS for ABA mitigation and thread-local caching.

## Files
- include/pool_allocator.hpp: Core LockFreePool implementation.
- tests/test_allocator.cpp: Unit tests and multi-threaded stress tests.
- benchmarks/benchmark_allocator.cpp: Benchmark suite profiling throughput and latency against std::malloc.
- REPORT.md: Standalone technical performance analysis report.
- CMakeLists.txt & Makefile: Build configuration.

## Build and Run

Using CMake:
mkdir build
cd build
cmake ..
cmake --build .
./test_allocator
./benchmark_allocator

Using Make:
make
./test_allocator
./benchmark_allocator
