#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>
#include <cstdlib>

namespace cultus {

class LockFreePool {
    struct Node {
        uint32_t next;
    };

    static constexpr uint32_t kNullIdx = 0xFFFFFFFF;
    static constexpr size_t kBatchSize = 32;

public:
    LockFreePool(size_t block_size, size_t block_count, size_t alignment = 64)
        : block_size_(align_up(block_size < sizeof(Node) ? sizeof(Node) : block_size, alignment)),
          block_count_(block_count),
          alignment_(alignment),
          allocated_count_(0) {
        size_t total_bytes = block_size_ * block_count_ + alignment_;
        raw_buffer_ = new char[total_bytes];
        
        uintptr_t addr = reinterpret_cast<uintptr_t>(raw_buffer_);
        uintptr_t aligned = (addr + alignment_ - 1) & ~(alignment_ - 1);
        pool_buffer_ = reinterpret_cast<char*>(aligned);

        for (size_t i = 0; i < block_count_; ++i) {
            Node* n = reinterpret_cast<Node*>(pool_buffer_ + i * block_size_);
            n->next = (i + 1 < block_count_) ? static_cast<uint32_t>(i + 1) : kNullIdx;
        }

        uint64_t head = (static_cast<uint64_t>(0) << 32) | static_cast<uint64_t>(0);
        global_head_.store(head, std::memory_order_release);
    }

    ~LockFreePool() {
        delete[] raw_buffer_;
    }

    LockFreePool(const LockFreePool&) = delete;
    LockFreePool& operator=(const LockFreePool&) = delete;

    void* allocate() {
        ThreadCache& tc = get_thread_cache();
        if (tc.count > 0) {
            tc.count--;
            allocated_count_.fetch_add(1, std::memory_order_relaxed);
            return tc.blocks[tc.count];
        }

        size_t fetched = refill_cache(tc);
        if (fetched == 0) {
            return nullptr;
        }

        tc.count--;
        allocated_count_.fetch_add(1, std::memory_order_relaxed);
        return tc.blocks[tc.count];
    }

    void deallocate(void* ptr) {
        if (!ptr || !is_from_pool(ptr)) {
            return;
        }

        ThreadCache& tc = get_thread_cache();
        if (tc.count < kBatchSize * 2) {
            tc.blocks[tc.count] = ptr;
            tc.count++;
            allocated_count_.fetch_sub(1, std::memory_order_relaxed);
            return;
        }

        flush_cache(tc);
        tc.blocks[tc.count] = ptr;
        tc.count++;
        allocated_count_.fetch_sub(1, std::memory_order_relaxed);
    }

    bool is_from_pool(const void* ptr) const noexcept {
        const char* p = reinterpret_cast<const char*>(ptr);
        return (p >= pool_buffer_) && (p < pool_buffer_ + block_count_ * block_size_);
    }

    size_t allocated_count() const noexcept {
        return allocated_count_.load(std::memory_order_relaxed);
    }

    size_t capacity() const noexcept {
        return block_count_;
    }

    size_t block_size() const noexcept {
        return block_size_;
    }

private:
    struct ThreadCache {
        void* blocks[kBatchSize * 2];
        size_t count{0};
    };

    static ThreadCache& get_thread_cache() {
        static thread_local ThreadCache cache;
        return cache;
    }

    size_t refill_cache(ThreadCache& tc) {
        size_t fetched = 0;
        uint64_t curr = global_head_.load(std::memory_order_acquire);

        while (fetched < kBatchSize) {
            uint32_t idx = static_cast<uint32_t>(curr & 0xFFFFFFFF);
            uint32_t tag = static_cast<uint32_t>(curr >> 32);

            if (idx == kNullIdx) {
                break;
            }

            Node* node = reinterpret_cast<Node*>(pool_buffer_ + idx * block_size_);
            uint32_t next_idx = node->next;
            uint64_t next_val = (static_cast<uint64_t>(tag + 1) << 32) | static_cast<uint64_t>(next_idx);

            if (global_head_.compare_exchange_weak(curr, next_val,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                tc.blocks[fetched++] = reinterpret_cast<void*>(node);
            }
        }

        tc.count = fetched;
        return fetched;
    }

    void flush_cache(ThreadCache& tc) {
        size_t to_flush = kBatchSize;
        for (size_t i = 0; i < to_flush; ++i) {
            void* ptr = tc.blocks[tc.count - 1 - i];
            uint32_t idx = static_cast<uint32_t>((reinterpret_cast<char*>(ptr) - pool_buffer_) / block_size_);
            Node* node = reinterpret_cast<Node*>(ptr);

            uint64_t curr = global_head_.load(std::memory_order_acquire);
            while (true) {
                uint32_t tag = static_cast<uint32_t>(curr >> 32);
                uint32_t head_idx = static_cast<uint32_t>(curr & 0xFFFFFFFF);

                node->next = head_idx;
                uint64_t next_val = (static_cast<uint64_t>(tag + 1) << 32) | static_cast<uint64_t>(idx);

                if (global_head_.compare_exchange_weak(curr, next_val,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_acquire)) {
                    break;
                }
            }
        }
        tc.count -= to_flush;
    }

    static size_t align_up(size_t val, size_t alignment) {
        return (val + alignment - 1) & ~(alignment - 1);
    }

    size_t block_size_;
    size_t block_count_;
    size_t alignment_;

    char* raw_buffer_;
    char* pool_buffer_;

    alignas(64) std::atomic<uint64_t> global_head_;
    alignas(64) std::atomic<size_t> allocated_count_;
};

}
