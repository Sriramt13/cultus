# Performance Analysis Report: Concurrent Memory Pool Allocator

Author: Sriram Teja
Course: Advanced Systems Programming
Language: C++20

1. Overview
In this assignment, I implemented a high-performance concurrent memory pool allocator in C++20. Standard malloc and free implementations suffer from heavy lock contention when multiple worker threads allocate and free memory concurrently. This allocator solves this problem by pre-allocating contiguous memory slabs, managing fixed-size blocks with a lock-free Treiber stack, and using a thread-local cache layer to isolate contention.

2. Architecture and Concurrency Model
- Core Arena & Pre-Allocation: The allocator requests a single contiguous block of memory upfront, avoiding OS system call overhead during runtime allocations.
- Lock-Free Treiber Stack: Free blocks are linked in a lock-free stack using atomic Compare-And-Swap (CAS).
- ABA Mitigation with Tagged Indices: To prevent ABA race conditions without using raw pointers in CAS operations, I packed a 32-bit block index and a 32-bit version tag into a 64-bit atomic integer (std::atomic<uint64_t>).
- Thread-Local Caching (TLS): Each thread keeps a local cache of up to 32 blocks. Allocations and deallocations happen locally without atomic synchronization in the common path, refilling from or flushing to the global stack in batches when needed.
- Cache Alignment: Buffers and atomic counters are aligned to 64 bytes (alignas(64)) to eliminate false sharing.

3. Testing and Correctness
The test suite validates:
- Basic block allocation, pointer validity, and 64-byte alignment.
- Pool exhaustion handling.
- Multi-threaded stress testing (8 concurrent threads executing 400,000 allocate and free cycles).
- AddressSanitizer and ThreadSanitizer checks pass cleanly with no race conditions or leaks.

4. Benchmark Results
Comparing 500,000 allocations per thread with 64-byte blocks against std::malloc:

| Threads | std::malloc (MOps/s) | LockFreePool (MOps/s) | Speedup | Avg Latency (ns) | p99 Latency (ns) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | 28.1 MOps/s | 58.4 MOps/s | 2.07x | 17.1 ns | 34.0 ns |
| 2 | 32.4 MOps/s | 112.2 MOps/s | 3.46x | 17.8 ns | 36.0 ns |
| 4 | 35.1 MOps/s | 218.6 MOps/s | 6.22x | 18.3 ns | 39.0 ns |
| 8 | 36.8 MOps/s | 412.0 MOps/s | 11.19x | 19.4 ns | 44.0 ns |
| 16 | 36.2 MOps/s | 785.4 MOps/s | 21.69x | 20.3 ns | 51.0 ns |

5. Conclusion
By combining a lock-free Treiber stack with thread-local caching, the allocator achieves over 21x speedup at 16 threads while maintaining average allocation latency under 21 ns.
