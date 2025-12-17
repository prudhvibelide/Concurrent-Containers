/*
 * main.cpp
 * Author: Prudhvi Raj Belide
 * Description: Test suite for concurrent containers
 */

#include "containers.h"
#include <iostream>
#include <thread>
#include <cassert>
#include <chrono>
#include <vector>
#include <string>
#include <atomic>

using namespace std;

/* Basic correctness tests */
void test_sgl_stack() {
    cout << "Testing SGL Stack... ";
    sgl_stack s;
    s.push(1); s.push(2); s.push(3);
    assert(s.pop() == 3 && s.pop() == 2 && s.pop() == 1);
    cout << "PASS" << endl;
}

void test_sgl_queue() {
    cout << "Testing SGL Queue... ";
    sgl_queue q;
    q.enqueue(1); q.enqueue(2); q.enqueue(3);
    assert(q.dequeue() == 1 && q.dequeue() == 2 && q.dequeue() == 3);
    cout << "PASS" << endl;
}

void test_treiber() {
    cout << "Testing Treiber Stack... ";
    treiber_stack s;
    s.push(1); s.push(2); s.push(3);
    assert(s.pop() == 3 && s.pop() == 2 && s.pop() == 1);
    cout << "PASS" << endl;
}

void test_msqueue() {
    cout << "Testing M&S Queue... ";
    msqueue q;
    q.enqueue(1); q.enqueue(2); q.enqueue(3);
    assert(q.dequeue() == 1 && q.dequeue() == 2 && q.dequeue() == 3);
    cout << "PASS" << endl;
}

void test_elimination() {
    cout << "Testing Elimination Stack... ";
    elimination_stack s;
    s.push(1); s.push(2); s.push(3);
    assert(s.pop() == 3 && s.pop() == 2 && s.pop() == 1);
    cout << "PASS" << endl;
}

void test_fc_stack() {
    cout << "Testing FC Stack... ";
    fc_stack s;
    s.push(1); s.push(2); s.push(3);
    assert(s.pop() == 3 && s.pop() == 2 && s.pop() == 1);
    cout << "PASS" << endl;
}

void test_fc_queue() {
    cout << "Testing FC Queue... ";
    fc_queue q;
    q.enqueue(1); q.enqueue(2); q.enqueue(3);
    assert(q.dequeue() == 1 && q.dequeue() == 2 && q.dequeue() == 3);
    cout << "PASS" << endl;
}

void test_condvar() {
    cout << "Testing Condition Variable... ";
    bounded_queue bq;
    
    thread producer([&]() {
        for(int i = 0; i < 50; i++) bq.enqueue(i);
    });
    
    thread consumer([&]() {
        for(int i = 0; i < 50; i++) bq.dequeue();
    });
    
    producer.join();
    consumer.join();
    cout << "PASS" << endl;
}

/* High contention test - all threads start simultaneously */
void test_contention() {
    cout << "\n=== Contention Test (8 threads) ===" << endl;
    
    treiber_stack s;
    atomic<bool> go(false);
    atomic<int> ready(0);
    
    auto worker = [&]() {
        ready++;
        while(!go.load()) { } // Wait for signal
        
        for(int i = 0; i < 5000; i++) {
            s.push(i);
            try { s.pop(); } catch(...) {}
        }
    };
    
    vector<thread> ts;
    for(int t = 0; t < 8; t++)
        ts.emplace_back(worker);
    
    while(ready.load() < 8) { }
    
    auto start = chrono::high_resolution_clock::now();
    go.store(true); // Release all threads at once
    
    for(auto& t : ts) t.join();
    auto end = chrono::high_resolution_clock::now();
    
    double ms = chrono::duration<double, milli>(end - start).count();
    cout << "Time: " << ms << " ms (all threads competed simultaneously)" << endl;
}

/* Benchmark a stack with multiple thread counts */
template<typename Stack>
static void bench_stack(const string& name, int threads, int ops_per_thread) {
    Stack s;

    for(int i = 0; i < threads * ops_per_thread; ++i)
        s.push(i);

    auto worker = [&](int id) {
        for(int i = 0; i < ops_per_thread; ++i) {
            if((i & 1) == 0)
                s.push(id * ops_per_thread + i);
            else {
                try { (void)s.pop(); } catch(...) {}
            }
        }
    };

    vector<thread> ts;
    auto start = chrono::high_resolution_clock::now();
    for(int t = 0; t < threads; ++t)
        ts.emplace_back(worker, t);
    for(auto& th : ts)
        th.join();
    auto end = chrono::high_resolution_clock::now();

    double secs = chrono::duration<double>(end - start).count();
    long long total_ops = 1LL * threads * ops_per_thread;
    double throughput = total_ops / secs;

    cout << "  " << name << "  threads=" << threads
              << "  ops=" << total_ops
              << "  throughput=" << throughput << " ops/s\n";
}

/* Benchmark a queue with producer/consumer threads */
template<typename Queue>
static void bench_queue(const string& name, int threads, int ops_per_thread) {
    Queue q;

    auto producer = [&](int id) {
        for(int i = 0; i < ops_per_thread; ++i)
            q.enqueue(id * ops_per_thread + i);
    };

    auto consumer = [&](int /*id*/) {
        for(int i = 0; i < ops_per_thread; ++i) {
            try { (void)q.dequeue(); } catch(...) {}
        }
    };

    int half = threads / 2;
    if(half == 0) half = 1;
    int prod_threads = half;
    int cons_threads = threads - half;
    if(cons_threads == 0) cons_threads = 1;

    vector<thread> ts;
    auto start = chrono::high_resolution_clock::now();
    for(int t = 0; t < prod_threads; ++t)
        ts.emplace_back(producer, t);
    for(int t = 0; t < cons_threads; ++t)
        ts.emplace_back(consumer, t);
    for(auto& th : ts)
        th.join();
    auto end = chrono::high_resolution_clock::now();

    double secs = chrono::duration<double>(end - start).count();
    long long total_ops = 1LL * ops_per_thread * (prod_threads + cons_threads);
    double throughput = total_ops / secs;

    cout << "  " << name << "  threads=" << threads
              << "  ops=" << total_ops
              << "  throughput=" << throughput << " ops/s\n";
}

/* Run all benchmarks */
static void run_benchmarks() {
    const int ops_per_thread = 100000;
    int thread_counts[] = {1, 2, 4, 8, 16};

    cout << "=== Stack Benchmarks ===\n";
    for(int t : thread_counts) {
        bench_stack<sgl_stack>("SGL Stack      ", t, ops_per_thread);
        bench_stack<treiber_stack>("Treiber Stack  ", t, ops_per_thread);
        bench_stack<elimination_stack>("Elimination Stk", t, ops_per_thread);
        bench_stack<fc_stack>("FC Stack       ", t, ops_per_thread);
    }

    cout << "\n=== Queue Benchmarks ===\n";
    for(int t : thread_counts) {
        bench_queue<sgl_queue>("SGL Queue      ", t, ops_per_thread);
        bench_queue<msqueue>("M&S Queue      ", t, ops_per_thread);
        bench_queue<fc_queue>("FC Queue       ", t, ops_per_thread);
    }
}

/* Print usage */
static void print_help(const char* prog) {
    cout << "Usage: " << prog << " [mode]\n\n";
    cout << "Modes:\n";
    cout << "  (no arguments)         Run unit tests\n";
    cout << "  -bench                 Run all benchmarks\n";
    cout << "  -contention            Run contention test\n";
    cout << "  -bench-sgl-stack       Benchmark SGL Stack only\n";
    cout << "  -bench-treiber         Benchmark Treiber Stack only\n";
    cout << "  -bench-elimination     Benchmark Elimination Stack only\n";
    cout << "  -bench-fc-stack        Benchmark FC Stack only\n";
    cout << "  -bench-sgl-queue       Benchmark SGL Queue only\n";
    cout << "  -bench-msqueue         Benchmark M&S Queue only\n";
    cout << "  -bench-fc-queue        Benchmark FC Queue only\n";
    cout << "  -h, --help             Show this help\n";
    cout << " \n";
    cout << "   For Perf : perf stat ./test_containers -bench\n"; 
}

int main(int argc, char** argv) {
    if(argc > 1) {
        string arg = argv[1];
        
        if(arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        }
        
        if(arg == "-bench") {
            cout << "=== Concurrent Containers Benchmarks ===\n";
            run_benchmarks();
            return 0;
        }
        
        if(arg == "-contention") {
            test_contention();
            return 0;
        }
        
        if(arg == "-bench-sgl-stack") {
            cout << "=== SGL Stack Only ===\n";
            int ops = 100000;
            for(int t : {1,2,4,8,16})
                bench_stack<sgl_stack>("SGL Stack", t, ops);
            return 0;
        }
        
        if(arg == "-bench-treiber") {
            cout << "=== Treiber Stack Only ===\n";
            int ops = 100000;
            for(int t : {1,2,4,8,16})
                bench_stack<treiber_stack>("Treiber Stack", t, ops);
            return 0;
        }
        
        if(arg == "-bench-elimination") {
            cout << "=== Elimination Stack Only ===\n";
            int ops = 100000;
            for(int t : {1,2,4,8,16})
                bench_stack<elimination_stack>("Elimination Stack", t, ops);
            return 0;
        }
        
        if(arg == "-bench-fc-stack") {
            cout << "=== FC Stack Only ===\n";
            int ops = 100000;
            for(int t : {1,2,4,8,16})
                bench_stack<fc_stack>("FC Stack", t, ops);
            return 0;
        }
        
        if(arg == "-bench-sgl-queue") {
            cout << "=== SGL Queue Only ===\n";
            int ops = 100000;
            for(int t : {1,2,4,8,16})
                bench_queue<sgl_queue>("SGL Queue", t, ops);
            return 0;
        }
        
        if(arg == "-bench-msqueue") {
            cout << "=== M&S Queue Only ===\n";
            int ops = 100000;
            for(int t : {1,2,4,8,16})
                bench_queue<msqueue>("M&S Queue", t, ops);
            return 0;
        }
        
        if(arg == "-bench-fc-queue") {
            cout << "=== FC Queue Only ===\n";
            int ops = 100000;
            for(int t : {1,2,4,8,16})
                bench_queue<fc_queue>("FC Queue", t, ops);
            return 0;
        }
    }
    
    // Default: run unit tests
    cout << "\n=== Concurrent Containers Test Cases ===" << endl;
    cout << "Author: Prudhvi Raj Belide\n" << endl;

    test_sgl_stack();
    test_sgl_queue();
    test_treiber();
    test_msqueue();
    test_elimination();
    test_fc_stack();
    test_fc_queue();
    test_condvar();

    cout << "\n=== ALL TESTS ARE PASSED ===" << endl;
    return 0;
}