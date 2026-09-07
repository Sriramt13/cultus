CXX ?= g++
CXXFLAGS ?= -O3 -std=c++20 -Wall -Wextra -pthread -Iinclude

all: test_allocator benchmark_allocator

test_allocator: tests/test_allocator.cpp include/pool_allocator.hpp
	$(CXX) $(CXXFLAGS) tests/test_allocator.cpp -o $@

benchmark_allocator: benchmarks/benchmark_allocator.cpp include/pool_allocator.hpp
	$(CXX) $(CXXFLAGS) benchmarks/benchmark_allocator.cpp -o $@

clean:
	rm -f test_allocator benchmark_allocator

.PHONY: all clean
