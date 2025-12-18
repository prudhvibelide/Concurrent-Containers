# Concurrent Containers – Final Project

**Author:** Prudhvi Raj Belide

## Overview

This project implements and evaluates seven concurrent data structures in **C++17**, with a focus on correctness, scalability, and performance under contention. The goal is to compare lock-based and lock-free designs and understand their behavior across different thread counts and workloads.

Four stack implementations and three queue implementations are included. The stack variants are a single global lock (SGL) stack using `std::mutex`, a lock-free Treiber stack based on atomic compare-and-swap, an elimination stack that augments the Treiber design with an elimination array, and a flat combining stack using the combiner-thread pattern.

The queue implementations include a single global lock (SGL) queue, a lock-free Michael & Scott queue using atomic head and tail pointers, and a flat combining queue.

As extra credit, the project also implements a condition variable that avoids spurious wakeups using an epoch counter, along with a bounded queue built on top of this custom condition variable.

All implementations rely exclusively on the C++17 standard library, including `std::atomic`, `std::mutex`, `std::thread`, and `std::condition_variable`.

---

## Code Organization

The file `containers.h` contains declarations for all container classes as well as shared constants such as `MAX_THREADS` and `ELIM_SIZE`.

The files `sgl_stack.cpp` and `sgl_queue.cpp` provide simple mutex-based implementations using a single global lock. These wrap `std::stack` and `std::queue` from the C++ standard library and use `std::lock_guard` for safe locking and unlocking.

The file `treiber_stack.cpp` implements a lock-free stack based on Treiber’s 1986 algorithm. It uses a single atomic pointer for the stack top and relies on `compare_exchange_weak` in retry loops. Nodes are not reclaimed, resulting in a memory leak due to the lack of a safe memory reclamation scheme.

The file `msqueue.cpp` contains a lock-free FIFO queue based on the Michael & Scott 1996 algorithm. It uses two atomic pointers (`head` and `tail`) and a dummy node to simplify empty queue handling. Threads help advance the tail pointer when it lags behind. Removed nodes are not reclaimed, which also leads to memory leaks.

The file `elimination_stack.cpp` extends the Treiber stack with an eight-slot elimination array. Threads attempt to pair push and pop operations in the elimination array before accessing the shared stack. Slot selection is randomized to reduce contention, with fallback to the Treiber stack if no match is found.

The files `fc_stack.cpp` and `fc_queue.cpp` implement flat combining versions of the stack and queue. Threads publish operation requests in per-thread slots, and one thread becomes the combiner to execute all pending operations. A mutex with `try_lock` is used to elect the combiner thread.

The file `condvar.cpp` implements `condvar_no_spurious`, a wrapper around `std::condition_variable` that avoids spurious wakeups by using an epoch counter. The `wait()` function only returns when the epoch changes. This file also includes a bounded queue implemented as a fixed-size circular buffer using two condition variables.

The file `main.cpp` contains unit tests for correctness, throughput benchmarks at 1, 2, 4, 8, and 16 threads, a contention test where all threads start simultaneously, and a command-line interface for selecting different test modes.

The `Makefile` compiles all source files using `-std=c++17 -pthread -O2 -Wall` and produces the `test_containers` executable.

---

## Compilation

```bash
make clean
make
````

Requires GCC 7+ or Clang 5+ with C++17 support and the pthread library.
Tested on Ubuntu 24.04.

---

## Running the Program

To run all correctness tests:

```bash
./test_containers
```

To run benchmarks:

```bash
./test_containers -bench
./test_containers -bench-treiber
./test_containers -bench-msqueue
perf stat ./test_containers -bench
```

Additional modes:

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

The M&S queue performs best at low thread counts and again at 8 threads. The SGL queue unexpectedly wins at 4 and 16 threads, a result that was repeatable. Flat combining is consistently the slowest due to slot scanning overhead.

---

### Contention Test

Eight threads performing 40,000 total operations (5,000 per thread) completed in **3.8 milliseconds** when all threads were released simultaneously, showing that the Treiber stack handles bursty contention reasonably well.

---

## Performance Analysis Using `perf`

Linux `perf` was used to analyze where time is spent in the lock-free implementations.

### Michael & Scott Queue

```bash
perf stat -d ./test_containers -bench-msqueue
```

The M&S queue achieves good parallelism with very low context switching, confirming its lock-free nature. However, the CPU spends most of its time stalled waiting for memory due to cache line bouncing on atomic head and tail pointers. The low instructions-per-cycle value confirms this memory-bound behavior. Page faults are caused by frequent node allocations, which leak due to missing reclamation.

---

### Treiber Stack

```bash
perf stat -d ./test_containers -bench-treiber
```

The Treiber stack is even more memory-bound than the M&S queue. All threads contend on a single atomic top pointer, causing heavy cache coherency traffic. Despite this, Treiber still outperforms mutex-based stacks because it allows parallel progress instead of serializing access.

---

## Known Issues

The Treiber stack and M&S queue leak memory because nodes are not safely reclaimed. Deleting nodes immediately after removal is unsafe since other threads may still access them. Proper solutions include hazard pointers or epoch-based reclamation, which were omitted for simplicity.

Some flat combining code paths rely on non-atomic flags. This is safe on x86 but may fail on weaker memory models such as ARM and should be corrected using atomics.

The elimination stack uses a fixed-size elimination array with random slot selection. Different sizes or selection strategies may improve performance, but in the current configuration the overhead outweighs the benefits.

---

## Conclusions

For this workload, the Treiber stack provides the best overall performance and scalability among stack implementations. The Michael & Scott queue is competitive but shows variability depending on thread count. Single global lock implementations are simple and effective at low contention, while elimination and flat combining do not provide benefits for this benchmark.

The `perf` analysis shows that lock-free algorithms achieve high parallelism and avoid blocking, but are fundamentally limited by memory system performance rather than computation. For production use, safe memory reclamation is essential.

