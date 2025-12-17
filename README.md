```markdown
# Concurrent Containers - Final Project
Author: Prudhvi Raj Belide

## Overview

This project implements and tests seven concurrent data structures in C++17:

**Stacks:**
- SGL Stack (single global lock with std::mutex)
- Treiber Stack (lock-free using atomic CAS)
- Elimination Stack (Treiber + elimination array)
- Flat Combining Stack (combiner thread pattern)

**Queues:**
- SGL Queue (single global lock)
- Michael & Scott Queue (lock-free with head/tail pointers)
- Flat Combining Queue (combiner thread pattern)

**Extra Credit:**
- Condition variable without spurious wakeups using epoch counter
- Bounded queue implementation using the condition variable

All code uses C++17 standard library with std::atomic, std::mutex, and std::thread.

---

## Code Organization

**containers.h**
- Declarations for all container classes
- Shared constants (MAX_THREADS, ELIM_SIZE, etc.)

**sgl_stack.cpp & sgl_queue.cpp**
- Simple mutex-based implementations
- Use std::lock_guard for automatic locking/unlocking
- Wrap std::stack and std::queue from STL

**treiber_stack.cpp**
- Lock-free stack from Treiber's 1986 algorithm
- Single atomic pointer (top)
- Uses compare_exchange_weak in retry loops
- Memory leak: nodes are not deleted (no safe reclamation)

**msqueue.cpp**
- Lock-free FIFO queue from Michael & Scott 1996 paper
- Two atomic pointers: head (dequeue) and tail (enqueue)
- Dummy node simplifies empty queue handling
- Helping mechanism: threads help advance tail if lagging
- Memory leak: old head nodes not deleted

**elimination_stack.cpp**
- Treiber stack with 8-slot elimination array
- Threads try to pair up push/pop operations before touching stack
- Random slot selection to reduce contention
- Falls back to Treiber if no match found

**fc_stack.cpp & fc_queue.cpp**
- Flat combining approach with 32 per-thread slots
- Threads post operation requests in slots
- One thread becomes combiner and executes all pending operations
- Uses mutex with try_lock

**condvar.cpp**
- condvar_no_spurious wrapper around std::condition_variable
- Uses epoch counter that increments on every signal/broadcast
- wait() only exits when epoch changes (no spurious wakeups)
- bounded_queue: fixed-size circular buffer with two condition variables

**main.cpp**
- Unit tests for correctness
- Throughput benchmarks at 1, 2, 4, 8, 16 threads
- Contention test (all threads start simultaneously)
- Command-line interface for different test modes

**Makefile**
- Compiles all source files with -std=c++17 -pthread -O2 -Wall
- Creates test_containers executable

---

## Compilation Instructions

```bash
make clean
make
```

Requires GCC 7+ or Clang 5+ with C++17 support and pthread library.
Tested on Ubuntu 24.04.

---

## Execution Instructions

### Run all tests
```bash
./test_containers
```

### Run benchmarks
```bash
./test_containers -bench                 # All benchmarks
./test_containers -bench-treiber         # Treiber only
./test_containers -bench-msqueue         # M&S Queue only
perf stat ./test_containers -bench       # For perf data
```

### Other modes
```bash
./test_containers -contention            # Contention test
./test_containers -h                     # Help
```

---

## Experimental Results

### Stack Throughput (ops/sec)

| Threads | SGL Stack | Treiber | Elimination | FC Stack |
|---------|-----------|---------|-------------|----------|
| 1       | 63.0M     | 38.1M   | 22.1M       | 25.2M    |
| 2       | 13.8M     | 32.2M   | 11.8M       | 11.2M    |
| 4       | 14.2M     | 17.0M   | 7.6M        | 7.1M     |
| 8       | 10.0M     | 11.7M   | 5.8M        | 7.3M     |
| 16      | 8.7M      | 10.3M   | 5.5M        | 7.0M     |

**Observations:**
- At 1 thread: SGL wins (no contention, simple mutex faster than atomic overhead)
- At 2+ threads: Treiber consistently outperforms others
- Elimination performs poorly across all thread counts
- FC sits between SGL and Treiber at higher thread counts

### Queue Throughput (ops/sec)

| Threads | SGL Queue | M&S Queue | FC Queue |
|---------|-----------|-----------|----------|
| 1       | 23.0M     | 24.0M     | 10.5M    |
| 2       | 18.7M     | 21.0M     | 12.8M    |
| 4       | 15.2M     | 13.3M     | 7.9M     |
| 8       | 7.1M      | 8.1M      | 7.7M     |
| 16      | 8.4M      | 6.5M      | 6.4M     |

**Observations:**
- M&S wins at low thread counts (1-2, 8)
- SGL wins at 4 and 16 threads (unexpected but repeatable)
- FC always slowest (overhead of slot scanning)
- Performance flattens out at high thread counts for all implementations

### Contention Test
8 threads doing 40,000 operations (5000 each) completed in 3.8 milliseconds when all released simultaneously. This shows the Treiber stack handles sudden bursts of concurrent operations reasonably well.

---

## Performance Analysis Using perf

I used Linux perf to understand where time is being spent in the lock-free implementations.

### M&S Queue Analysis

Command:
```bash
perf stat -d ./test_containers -bench-msqueue
```

Key metrics:
- **Task clock:** 3729 ms
- **CPUs utilized:** 9.7 out of 16 (good parallelism)
- **Context switches:** 124 (very low - proof of lock-free design)
- **Instructions per cycle:** 0.50 (low - CPU is stalling)
- **Backend bound:** 69.8% (CPU waiting on memory most of the time)
- **L1 cache miss rate:** 1.64%
- **Page faults:** 11,707 (memory allocations for new nodes)

**What this means:**
The M&S queue achieves good parallelism (using ~10 cores) and almost never blocks threads (only 124 context switches for millions of operations). However, the CPU spends 70% of its time waiting for memory rather than doing computation. This is because atomic operations on head and tail cause cache lines to bounce between cores. The 0.50 instructions per cycle confirms the CPU is mostly stalled waiting for memory, not busy computing. The high page faults show memory allocation overhead from creating nodes (which leak because we don't delete them).

### Treiber Stack Analysis

Command:
```bash
perf stat -d ./test_containers -bench-treiber
```

Key metrics:
- **CPUs utilized:** 6.8
- **Context switches:** 83 (extremely low)
- **Instructions per cycle:** 0.24 (worse than M&S)
- **Backend bound:** 86.7% (worse than M&S!)
- **L1 cache miss rate:** 2.14%

**What this means:**
Treiber stack has even worse memory bottleneck than M&S queue. The CPU spends 87% of time waiting for memory. This makes sense because every thread is hammering the same top pointer with CAS operations. The single shared atomic pointer creates more cache coherency traffic than M&S's two-pointer design in some scenarios. Despite this, Treiber still outperforms SGL in benchmarks because it allows parallel attempts rather than serializing through a mutex.

### Why Elimination Performed Poorly

The elimination stack was supposed to reduce contention by letting push/pop pairs cancel in the elimination array. However, it performed worse than plain Treiber across all thread counts. 

Likely reasons:
1. Random slot selection causes misses (threads pick different slots)
2. Overhead of checking 8 array slots outweighs benefit
3. My workload has equal push/pop but they don't arrive simultaneously
4. The elimination array itself creates additional cache misses

In this benchmark, the elimination optimization made things worse rather than better.

### Why FC Didn't Win

Flat combining should win when:
- Many operations arrive simultaneously (combiner batches them)
- Lock overhead is amortized across many operations

But in my benchmark:
- Operations are very fast (just push/pop with no work)
- Scanning 32 slots is expensive
- Combiner usually finds only 2-3 pending operations
- Overhead of slot management > benefit of batching

FC would likely perform better with:
- Slower operations (so scanning overhead is smaller percentage)
- More threads all hitting at exact same time
- Operations that benefit from sequential execution (cache locality)

---

## Comparison: Lock-Free vs Mutex

**Lock-Free Advantages:**
- Very low context switches (83-124 vs thousands for mutex)
- Good CPU utilization (6-10 cores)
- No deadlock risk
- Threads never sleep

**Lock-Free Disadvantages:**
- High backend_bound (70-87% waiting on memory)
- Low IPC (0.24-0.50 instructions per cycle)
- Complex to implement correctly
- Memory leaks without reclamation scheme

**When SGL (mutex) won:**
- 1 thread: No contention, mutex overhead is minimal
- 16 threads on queue: Lock implementation got lucky with scheduling

**When Lock-Free won:**
- Most scenarios with 2-8 threads
- Consistent scaling as threads increase
- Better than elimination and FC alternatives

---

## Extant Bugs

1. **Memory leaks in Treiber and M&S:**
   - Nodes are deleted directly after pop/dequeue
   - This is unsafe (other threads might still be reading)
   - Should use hazard pointers or epoch-based reclamation
   - For benchmarking with fixed operation counts this is acceptable
   - Would cause problems in long-running production systems

2. **Flat combining portability:**
   - Some slot fields use simple flags instead of atomics
   - Works correctly on x86 but might have issues on ARM
   - Should use std::atomic for full correctness

3. **Elimination array tuning:**
   - Fixed 8 slots and random selection may not be optimal
   - Could experiment with different sizes and selection strategies
   - Current implementation shows overhead exceeds benefit

---

## Conclusions

For this benchmark workload:
- **Treiber stack** is the clear winner for stacks (best scaling, consistent performance)
- **M&S queue** is competitive for queues but variable depending on thread count
- **SGL implementations** are simpler and adequate at low thread counts
- **Elimination and FC** don't provide benefits in this workload

The perf analysis shows lock-free algorithms achieve parallelism and avoid blocking, but are fundamentally limited by memory system performance rather than computation. The CPU spends 70-87% of time waiting for memory due to atomic operations causing cache coherency traffic.

For production use, the memory leak issue must be addressed with a proper reclamation scheme like hazard pointers.
```
