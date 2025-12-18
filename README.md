# Concurrent Containers – Final Project

**Author:** Prudhvi Raj Belide

## Overview

This project implements and evaluates seven concurrent data structures in **C++17**, focusing on correctness, scalability, and performance under contention.

The following data structures are implemented and tested.

**Stacks**
- SGL Stack (single global lock using `std::mutex`)
- Treiber Stack (lock-free using atomic CAS)
- Elimination Stack (Treiber stack with elimination array)
- Flat Combining Stack (combiner-thread pattern)

**Queues**
- SGL Queue (single global lock)
- Michael & Scott Queue (lock-free queue with atomic head and tail)
- Flat Combining Queue (combiner-thread pattern)

**Extra Credit**
- Condition variable implementation without spurious wakeups using an epoch counter
- Bounded queue built using the custom condition variable

All implementations use the C++17 standard library, including `std::atomic`, `std::mutex`, `std::thread`, and `std::condition_variable`.

---

## Code Organization

**containers.h**  
Contains declarations for all container classes and shared constants such as `MAX_THREADS` and `ELIM_SIZE`.

**sgl_stack.cpp / sgl_queue.cpp**  
Mutex-based implementations using a single global lock. These wrap `std::stack` and `std::queue` from the STL and use `std::lock_guard` for safe locking.

**treiber_stack.cpp**  
Lock-free stack based on Treiber’s 1986 algorithm. It uses a single atomic pointer for the top and relies on `compare_exchange_weak` in retry loops. Nodes are not reclaimed, resulting in a memory leak due to the lack of safe memory reclamation.

**msqueue.cpp**  
Lock-free FIFO queue based on the Michael & Scott 1996 algorithm. It uses two atomic pointers (`head` and `tail`) and a dummy node to simplify empty queue handling. Threads help advance the tail pointer when it lags behind. Old nodes are not deleted, leading to memory leaks.

**elimination_stack.cpp**  
Extends the Treiber stack with an 8-slot elimination array. Threads attempt to pair push and pop operations in the elimination array before accessing the shared stack. Random slot selection is used to reduce contention, with fallback to Treiber behavior if no match is found.

**fc_stack.cpp / fc_queue.cpp**  
Flat combining implementations using 32 per-thread slots. Threads publish operation requests, and one thread becomes the combiner to execute all pending operations. A mutex with `try_lock` is used to elect the combiner.

**condvar.cpp**  
Implements `condvar_no_spurious`, a wrapper around `std::condition_variable` that avoids spurious wakeups using an epoch counter. The `wait()` function only returns when the epoch changes. Also includes a bounded queue implemented as a fixed-size circular buffer using two condition variables.

**main.cpp**  
Contains unit tests for correctness, throughput benchmarks at 1, 2, 4, 8, and 16 threads, a contention test where all threads start simultaneously, and a command-line interface to select test modes.

**Makefile**  
Builds all source files using `-std=c++17 -pthread -O2 -Wall` and produces the `test_containers` executable.

---

## Compilation Instructions

make clean
make
````

Requires GCC 7+ or Clang 5+ with C++17 support and the pthread library.
Tested on Ubuntu 24.04.

---

## Execution Instructions

### Run all tests

```bash
./test_containers
```

### Run benchmarks

```bash
./test_containers -bench
./test_containers -bench-treiber
./test_containers -bench-msqueue
perf stat ./test_containers -bench
```

### Other modes

```bash
./test_containers -contention
./test_containers -h
```

---

## Experimental Results

### Stack Throughput (operations per second)

| Threads | SGL Stack | Treiber | Elimination | FC Stack |
| ------: | --------: | ------: | ----------: | -------: |
|       1 |     63.0M |   38.1M |       22.1M |    25.2M |
|       2 |     13.8M |   32.2M |       11.8M |    11.2M |
|       4 |     14.2M |   17.0M |        7.6M |     7.1M |
|       8 |     10.0M |   11.7M |        5.8M |     7.3M |
|      16 |      8.7M |   10.3M |        5.5M |     7.0M |

**Observations**

At one thread, the SGL stack performs best because there is no contention and mutex overhead is minimal compared to atomic operations. With two or more threads, the Treiber stack consistently outperforms the others. The elimination stack performs poorly at all thread counts, while the flat combining stack sits between SGL and Treiber at higher thread counts.

---

### Queue Throughput (operations per second)

| Threads | SGL Queue | M&S Queue | FC Queue |
| ------: | --------: | --------: | -------: |
|       1 |     23.0M |     24.0M |    10.5M |
|       2 |     18.7M |     21.0M |    12.8M |
|       4 |     15.2M |     13.3M |     7.9M |
|       8 |      7.1M |      8.1M |     7.7M |
|      16 |      8.4M |      6.5M |     6.4M |

**Observations**

The M&S queue performs best at low thread counts and again at 8 threads. The SGL queue unexpectedly wins at 4 and 16 threads, a result that was repeatable. Flat combining is consistently the slowest due to slot scanning overhead. Performance for all implementations flattens at high thread counts.

---

### Contention Test

Eight threads performing 40,000 total operations (5,000 per thread) completed in **3.8 milliseconds** when all threads were released simultaneously. This demonstrates that the Treiber stack handles sudden bursts of contention reasonably well.

---

## Performance Analysis Using `perf`

Linux `perf` was used to analyze where time is spent in the lock-free implementations.

### Michael & Scott Queue

Command:

```bash
perf stat -d ./test_containers -bench-msqueue
```

Key metrics:

* Task clock: 3729 ms
* CPUs utilized: 9.7 out of 16
* Context switches: 124
* Instructions per cycle: 0.50
* Backend bound: 69.8%
* L1 cache miss rate: 1.64%
* Page faults: 11,707

**Analysis**

The M&S queue achieves good parallelism and very low context switching, confirming its lock-free nature. However, the CPU spends most of its time stalled waiting for memory due to cache line bouncing on atomic head and tail pointers. The low IPC confirms this memory-bound behavior. Page faults are caused by frequent node allocations, which leak due to missing reclamation.

---

### Treiber Stack

Command:

```bash
perf stat -d ./test_containers -bench-treiber
```

Key metrics:

* CPUs utilized: 6.8
* Context switches: 83
* Instructions per cycle: 0.24
* Backend bound: 86.7%
* L1 cache miss rate: 2.14%

**Analysis**

The Treiber stack is even more memory-bound than the M&S queue. All threads contend on a single atomic top pointer, causing heavy cache coherency traffic. Despite this, Treiber still outperforms mutex-based stacks because it allows parallel progress rather than serializing access.

---

## Why Elimination Performed Poorly

The elimination stack was intended to reduce contention by allowing push and pop operations to cancel each other. In practice, it performed worse than the plain Treiber stack for several reasons. Random slot selection often leads to missed pairings, checking multiple slots adds overhead, operations do not arrive simultaneously enough to benefit from elimination, and the elimination array itself introduces additional cache misses.

---

## Why Flat Combining Did Not Win

Flat combining performs best when many operations arrive at once and individual operations are expensive. In this benchmark, operations are extremely fast, slot scanning is costly, and the combiner typically finds only a few pending operations. The overhead of managing slots outweighs the benefit of batching. Flat combining would likely perform better with slower operations, higher synchronization, or workloads with stronger temporal locality.

---

## Lock-Free vs Mutex-Based Designs

Lock-free implementations show extremely low context switching and good CPU utilization, with no risk of deadlock and no sleeping threads. However, they are heavily memory-bound, exhibit very low IPC, are complex to implement correctly, and require careful memory reclamation to avoid leaks.

Mutex-based SGL implementations perform well with a single thread and occasionally outperform lock-free designs at high thread counts due to favorable scheduling. Lock-free designs dominate most scenarios with moderate thread counts and scale more consistently.

---

## Known Issues

The Treiber stack and M&S queue leak memory because nodes are not safely reclaimed. Deleting nodes immediately after removal is unsafe because other threads may still access them. Proper solutions include hazard pointers or epoch-based reclamation, which were omitted for simplicity in benchmarking.

The flat combining implementations rely on non-atomic flags in some places. This is safe on x86 but may break on weaker memory models such as ARM and should be fixed using atomics.

The elimination stack uses a fixed-size elimination array with random selection. Different sizes or selection strategies may improve performance, but in the current configuration the overhead outweighs the benefits.

---

## Conclusions

For this workload, the Treiber stack provides the best overall performance and scalability among stack implementations. The Michael & Scott queue is competitive but shows variability depending on thread count. Single global lock implementations are simple and effective at low contention. Elimination and flat combining techniques do not provide benefits for this benchmark.

The `perf` analysis shows that lock-free algorithms achieve high parallelism and avoid blocking but are fundamentally limited by memory system performance rather than computation. For production systems, safe memory reclamation is essential.

