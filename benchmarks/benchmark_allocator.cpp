#include "pool_allocator.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <algorithm>
#include <numeric>

struct BenchmarkMetrics {
    double throughput_mops;
    double avg_latency_ns;
    double p50_latency_ns;
    double p99_latency_ns;
};

template <typename AllocFn, typename FreeFn>
BenchmarkMetrics run_benchmark(AllocFn alloc_fn, FreeFn free_fn, int num_threads, int ops_per_thread) {
    std::vector<std::thread> threads;
    std::vector<std::vector<double>> thread_latencies(num_threads);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, ops_per_thread, alloc_fn, free_fn, &thread_latencies]() {
            std::vector<void*> ptrs;
            ptrs.reserve(128);
            std::vector<double> latencies;
            latencies.reserve(ops_per_thread / 10);

            for (int i = 0; i < ops_per_thread; ++i) {
                auto t0 = std::chrono::high_resolution_clock::now();
                void* p = alloc_fn();
                auto t1 = std::chrono::high_resolution_clock::now();

                if (i % 10 == 0) {
                    latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
                }

                if (p) ptrs.push_back(p);

                if (ptrs.size() >= 128) {
                    for (void* ptr : ptrs) {
                        free_fn(ptr);
                    }
                    ptrs.clear();
                }
            }

            for (void* ptr : ptrs) {
                free_fn(ptr);
            }

            thread_latencies[t] = std::move(latencies);
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    double total_ops = static_cast<double>(num_threads * ops_per_thread);
    double mops = (total_ops / (total_ms / 1000.0)) / 1e6;

    std::vector<double> all_latencies;
    for (auto& tl : thread_latencies) {
        all_latencies.insert(all_latencies.end(), tl.begin(), tl.end());
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    double sum = std::accumulate(all_latencies.begin(), all_latencies.end(), 0.0);
    double avg_lat = all_latencies.empty() ? 0 : sum / all_latencies.size();
    double p50_lat = all_latencies.empty() ? 0 : all_latencies[all_latencies.size() * 50 / 100];
    double p99_lat = all_latencies.empty() ? 0 : all_latencies[all_latencies.size() * 99 / 100];

    return {mops, avg_lat, p50_lat, p99_lat};
}

int main() {
    std::cout << "===============================================================================\n";
    std::cout << "      CONCURRENT MEMORY POOL (LOCK-FREE + TLS) VS MALLOC BENCHMARK             \n";
    std::cout << "===============================================================================\n\n";

    const std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    const int ops_per_thread = 500000;
    const size_t pool_capacity = 65536;

    std::cout << std::left
              << std::setw(10) << "Threads"
              << std::setw(20) << "malloc (MOps/s)"
              << std::setw(22) << "Pool (MOps/s)"
              << std::setw(14) << "Speedup"
              << std::setw(14) << "Avg(ns)"
              << std::setw(14) << "p99(ns)" << "\n";
    std::cout << std::string(94, '-') << "\n";

    for (int t : thread_counts) {
        cultus::LockFreePool pool(64, pool_capacity, 64);

        auto sys_res = run_benchmark(
            []() { return std::malloc(64); },
            [](void* p) { std::free(p); },
            t, ops_per_thread
        );

        auto pool_res = run_benchmark(
            [&]() { return pool.allocate(); },
            [&](void* p) { pool.deallocate(p); },
            t, ops_per_thread
        );

        double speedup = pool_res.throughput_mops / sys_res.throughput_mops;

        std::cout << std::left
                  << std::setw(10) << t
                  << std::setw(20) << std::fixed << std::setprecision(2) << sys_res.throughput_mops
                  << std::setw(22) << std::fixed << std::setprecision(2) << pool_res.throughput_mops
                  << std::setw(14) << std::fixed << std::setprecision(2) << speedup << "x"
                  << std::setw(14) << std::fixed << std::setprecision(1) << pool_res.avg_latency_ns
                  << std::setw(14) << std::fixed << std::setprecision(1) << pool_res.p99_latency_ns << "\n";
    }

    std::cout << std::string(94, '-') << "\n";
    return 0;
}
