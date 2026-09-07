#include "pool_allocator.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <cassert>
#include <cstring>
#include <atomic>

void test_basic() {
    cultus::LockFreePool pool(64, 16, 64);

    assert(pool.capacity() == 16);
    assert(pool.allocated_count() == 0);

    std::vector<void*> ptrs;
    for (size_t i = 0; i < 16; ++i) {
        void* p = pool.allocate();
        assert(p != nullptr);
        assert(pool.is_from_pool(p));
        assert((reinterpret_cast<uintptr_t>(p) % 64) == 0);
        ptrs.push_back(p);
    }

    assert(pool.allocate() == nullptr);

    for (void* p : ptrs) {
        pool.deallocate(p);
    }

    std::cout << "[PASS] Basic allocation, free, and alignment checks\n";
}

void test_multithreaded_stress() {
    const size_t capacity = 4096;
    cultus::LockFreePool pool(64, capacity, 64);

    const int num_threads = 8;
    const int ops_per_thread = 50000;
    std::vector<std::thread> threads;
    std::atomic<bool> start_flag{false};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::vector<void*> local_ptrs;
            local_ptrs.reserve(64);

            while (!start_flag.load(std::memory_order_relaxed)) {
                std::this_thread::yield();
            }

            for (int i = 0; i < ops_per_thread; ++i) {
                void* p = pool.allocate();
                if (p) {
                    *reinterpret_cast<int*>(p) = i + t;
                    local_ptrs.push_back(p);
                }

                if (local_ptrs.size() >= 32 || (!p && !local_ptrs.empty())) {
                    for (void* ptr : local_ptrs) {
                        pool.deallocate(ptr);
                    }
                    local_ptrs.clear();
                }
            }

            for (void* ptr : local_ptrs) {
                pool.deallocate(ptr);
            }
        });
    }

    start_flag.store(true);
    for (auto& th : threads) {
        th.join();
    }

    std::cout << "[PASS] Concurrent multi-threaded stress test (8 threads, 400k ops)\n";
}

int main() {
    std::cout << "Running Concurrent Memory Pool Tests...\n\n";
    test_basic();
    test_multithreaded_stress();
    std::cout << "\nAll test assertions passed.\n";
    return 0;
}
