# High-Performance Order Book Implementation

A low-latency, high-throughput order book implementation in C++20 designed for financial trading systems. This project focuses on microsecond-level performance optimization through careful design choices in memory management, data structures, and algorithmic approaches.

## Table of Contents
- [Features](#features)
- [Quick Start](#quick-start)
- [Build System](#build-system)
- [Usage](#usage)
- [Performance Analysis](#performance-analysis)
- [Design Choices for Latency Optimization](#design-choices-for-latency-optimization)
- [Architecture Overview](#architecture-overview)
- [Testing](#testing)
- [Contributing](#contributing)

## Features

- **Ultra-low latency**: Nanosecond-precision timing with optimized hot paths
- **Memory-efficient**: Custom memory pools with intrusive data structures
- **Thread-safe**: Concurrent access support with minimal locking
- **Real-time performance monitoring**: Built-in latency statistics and profiling
- **Interactive CLI**: Colored terminal interface for live order book interaction
- **Comprehensive testing**: Full test suite with multiple order scenarios
- **Modern C++20**: Leverages latest language features for performance

## Quick Start

### Prerequisites
- GCC 15.2+ or Clang 18+ with C++20 support
- CMake 3.16+
- Google Test (for testing)
- Boost libraries (for intrusive containers)

### Build and Run
```bash
# Clone the repository
git clone <repository-url>
cd Order-Book

# Run tests
make test

# Run interactive application
make app

# Run performance benchmarks
make performance

# Clean build artifacts
make clean
```

## Build System

The project uses a dual build system approach:

### CMake (Primary)
- **Development builds**: `make test`, `make app`
- **Optimized builds**: Automatic Release mode with high-performance flags
- **Cross-platform**: Works on Linux, macOS, Windows

### Direct Compilation (Performance)
- **Benchmark builds**: `make performance`
- **Maximum optimization**: Direct g++ with `-O3 -march=native -flto=auto`
- **Profiling-friendly**: Includes `-fno-omit-frame-pointer` for better profiling

### Compiler Flags (Performance Build)
```bash
-std=gnu++20           # C++20 with GNU extensions
-O3                    # Maximum optimization
-DNDEBUG              # Disable assertions
-march=native         # CPU-specific optimizations
-flto=auto            # Link-time optimization
-fno-omit-frame-pointer  # Better profiling support
-pipe                 # Faster compilation
-pthread              # Thread support
```

## Usage

### Interactive Application
```bash
make app
```

The interactive application provides a colored terminal interface:

#### Commands
- `A <side> <orderType> <price> <quantity> [orderId]` - Add order
- `C <orderId>` - Cancel order  
- `M <orderId> <side> <price> <quantity>` - Modify order
- `S` - Show order book status
- `H` - Help
- `Q` - Quit

#### Order Types
- **GTC** (Good Till Cancel) - Remains until explicitly cancelled
- **GFD** (Good For Day) - Automatically cancelled at day end
- **FAK** (Fill And Kill) - Fill immediately, cancel remainder
- **FOK** (Fill Or Kill) - Fill completely or cancel entirely
- **MKT** (Market) - Execute at best available price

#### Examples
```bash
OrderBook[1000]> A B GTC 100.50 1000     # Buy 1000 @ $100.50
OrderBook[1001]> A S GTC 100.75 500      # Sell 500 @ $100.75
OrderBook[1002]> A B MKT 0 200            # Market buy 200 shares
OrderBook[1003]> S                        # Show order book
OrderBook[1003]> C 1000                   # Cancel order 1000
```

### Performance Testing
```bash
make performance
```

Populates the OB with Millions of orders in steady state and Runs comprehensive latency benchmarks:
- Order insertion performance
- Matching engine latency
- Memory allocation overhead
- Statistical analysis (median, 95th, 99th, 99.99th percentiles)
- Main benchmarkig done from `src/PerfomarmanceTesting.cpp` script where we observe the behaviour of the OB when serving Millions of orders. This gives us a clear picture of a real world setting with warmer caches and lower latencies.
### Latency Statistics
The performance test generates detailed statistics for time for action(in ns):
```
Stats for initial BUY population:
samples: 1000000
average:  115.802
median:  120
95th:    140
96th:    140
97th:    141
98th:    150
99th:    161
99.99th: 4108

```

### Unit Testing
```bash
make test
```

Runs the complete test suite covering:
- Order matching scenarios
- Edge cases (Fill-or-Kill, Fill-and-Kill)
- Market orders
- Order modifications
- Cancellation handling

## Performance Analysis

The application provides real-time performance metrics:

### Core Metrics
- **Order Processing Time**: Nanosecond precision for add/cancel/modify operations
- **Matching Latency**: Time to match orders and generate trades
- **Memory Pool Efficiency**: Allocation/deallocation overhead

### Example Output
```
✓ Order 1000 added successfully.
Trades executed (2):
Trade: Buy Order #1000 matched with sell order #1001 at Price=100.50 and Quantity=500
Core OrderBook time: 1,247 ns (1.25 μs)
```

## Design Choices for Latency Optimization

### 1. Memory Management Strategy

#### Custom Memory Pools
**Problem**: Standard `malloc`/`new` introduces unpredictable latency spikes due to system calls and fragmentation.

**Solution**: Pre-allocated memory pools with intrusive free lists.
```cpp
MemoryPool<Order> order_pool(1'000'000);  // Pre-allocate 1M orders
```

**Benefits**:
- **Deterministic allocation**: O(1) allocation/deallocation
- **Cache efficiency**: Contiguous memory layout
- **No system calls**: Eliminates kernel interaction in hot path
- **Fragmentation-free**: Controlled memory layout

#### Intrusive Reference Counting
**Problem**: `std::shared_ptr` control blocks require separate allocations.

**Solution**: Intrusive reference counting with `boost::intrusive_ptr`.
```cpp
class Order {
    mutable std::atomic<int> ref_count_{1};  // Embedded in object
    // ...
};
```

**Benefits**:
- **Zero allocation overhead**: No separate control block
- **Better cache locality**: Reference count co-located with data
- **Atomic operations**: Lock-free reference management

### 2. Data Structure Selection

#### Price-Level Containers
**Choice**: `std::map<Price, OrderList>` for price levels
- **Sorted access**: O(log n) insertion, automatic price ordering
- **Iterator stability**: Safe iteration during modifications
- **Memory efficiency**: Balanced tree structure

#### Order Lookup
**Evolution**: `std::unordered_map` → `boost::unordered_flat_map` → `tsl::robin_map`
- **Hash table optimization**: Open addressing for better cache performance
- **Robin Hood hashing**: Minimizes worst-case probe distance
- **Memory layout**: Contiguous storage reduces cache misses

#### Order Lists per Price Level
**Choice**: Custom doubly-linked list with memory pool backing
```cpp
template<typename T>
class CustomLinkedList {
    std::shared_ptr<MemoryPool<ListNode<T>>> pool_;
    // O(1) insertion/deletion with pooled nodes
};
```

**Benefits**:
- **O(1) operations**: Constant-time insertion/deletion anywhere
- **Iterator stability**: Pointers remain valid during modifications
- **Memory pooling**: Eliminates allocation overhead

### 3. Algorithmic Optimizations

#### Hot Path Optimization
**Strategy**: Minimize work in the critical matching loop
```cpp
TradeInfos OrderBook::match_orders() {
    // Cache frequently accessed data
    auto &bids_pair = *bids_.begin();
    auto &asks_pair = *asks_.begin();
    auto &bids_list = bids_pair.second;
    auto &asks_list = asks_pair.second;
    
    // Minimize pointer dereferencing
    auto &bid_order = **bids_list.begin();
    auto &ask_order = **asks_list.begin();
}
```

#### Branch Prediction Optimization
```cpp
  if (orders_.find(id) != orders_.end()) [[unlikely]] {
    return {};
  }
```

#### SIMD-Friendly Operations
- Contiguous memory layout enables vectorization
- Aligned data structures for optimal memory access
- Minimal branching in inner loops

### 4. Concurrency Design

#### Lock-Free Operations Where Possible
- Atomic reference counting for thread-safe object lifecycle
- Memory pools designed for concurrent access patterns
- Read-mostly data structures optimized for shared access

#### Strategic Locking
```cpp
std::scoped_lock ordersLock{ordersMutex_};  // RAII locking
// Minimal critical section
```

**Principles**:
- **Short critical sections**: Hold locks for minimum time
- **Coarse-grained locking**: Reduce lock contention overhead
- **Reader-writer separation**: Where applicable

### 5. Compiler and Hardware Optimization

#### CPU-Specific Optimizations
```bash
-march=native    # Use all available CPU instructions
-mtune=native    # Optimize for specific CPU microarchitecture
```

#### Link-Time Optimization
```bash
-flto=auto       # Cross-module optimization
```

#### Cache Optimization Strategies
- **Data locality**: Related data stored together
- **Cache line alignment**: Avoid false sharing
- **Prefetching**: Hint processor about future memory access

#### Branch Prediction
- **Likely/unlikely hints**: Guide processor speculation
- **Consistent branching patterns**: Improve prediction accuracy

### 6. Profiling and Measurement

#### High-Resolution Timing
```cpp
uint64_t get_time_nanoseconds() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
}
```

#### Fine-Grained Profiling
- **Step-by-step timing**: Identify bottlenecks within operations
- **Statistical analysis**: Understand latency distributions
- **Real-time monitoring**: Continuous performance feedback

### 7. System-Level Optimizations

#### Process Isolation
- CPU affinity binding for consistent performance
- Real-time scheduling priorities
- Memory locking to prevent swapping

#### I/O Minimization
- Batch logging operations
- Asynchronous I/O where possible
- Memory-mapped files for persistent data

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                           OrderBookApp                          │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐    ┌─────────────────┐                    │
│  │  MemoryPool<T>  │    │    OrderBook    │                    │
│  │                 │    │                 │                    │
│  │ ┌─────────────┐ │    │ ┌─────────────┐ │                    │
│  │ │ Chunk 1     │ │    │ │    bids_    │ │                    │
│  │ │ Chunk 2     │ │◄───┤ │    asks_    │ │                    │
│  │ │ ...         │ │    │ │   orders_   │ │                    │
│  │ │ Free List   │ │    │ └─────────────┘ │                    │
│  │ └─────────────┘ │    │                 │                    │
│  └─────────────────┘    └─────────────────┘                    │
└─────────────────────────────────────────────────────────────────┘

Data Flow:
1. Order Creation → Memory Pool Allocation
2. Order Insertion → Price Level Management  
3. Order Matching → Trade Generation
4. Order Cleanup → Memory Pool Deallocation
```

## Testing

### Test Categories
- **Functional Tests**: Verify correct order matching behavior
- **Performance Tests**: Measure latency and throughput
- **Stress Tests**: High-volume order processing
- **Edge Case Tests**: Boundary conditions and error handling

### Running Tests
```bash
make test           # Run all tests
make app-debug      # Debug build with assertions
make clean          # Clean all artifacts
```

## Contributing

### Performance Guidelines
1. **Measure first**: Profile before optimizing
2. **Hot path focus**: Optimize the critical 1% of code
3. **Memory awareness**: Consider cache effects
4. **Benchmark changes**: Quantify performance impact

### Code Style
- Modern C++20 features preferred
- RAII for resource management
- Const-correctness throughout
- Clear variable naming for performance-critical code

---

**Note**: This implementation prioritizes latency over throughput. For maximum throughput scenarios, consider batching operations and relaxing some real-time constraints.


## Contact
hrushikeshkalikiri@gmail.com(email)
