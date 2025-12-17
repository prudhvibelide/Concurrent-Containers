/*
 * containers.h
 * Author: Prudhvi Raj Belide
 *
 * Description: Header file for all concurrent container classes.
 */

#ifndef CONTAINERS_H
#define CONTAINERS_H

#include <mutex>
#include <stack>
#include <queue>
#include <atomic>
#include <vector>
#include <stdexcept>
#include <condition_variable>

#define ELIM_SIZE 8
#define MAX_THREADS 32

/* Single global lock stack */
class sgl_stack {
    std::stack<int> data;
    std::mutex lock;
public:
    void push(int value);
    int pop();
};

/* Single global lock queue */
class sgl_queue {
    std::queue<int> data;
    std::mutex lock;
public:
    void enqueue(int value);
    int dequeue();
};

/* Treiber lock-free stack */
class treiber_stack {
    struct node {
        int value;
        node* next;
        node(int v) : value(v), next(nullptr) {}
    };
    std::atomic<node*> top;
public:
    treiber_stack() : top(nullptr) {}
    ~treiber_stack();
    void push(int value);
    int pop();
};

/* Michael & Scott lock-free queue */
class msqueue {
    struct node {
        int value;
        std::atomic<node*> next;
        node(int v) : value(v), next(nullptr) {}
    };
    std::atomic<node*> head;
    std::atomic<node*> tail;
public:
    msqueue();
    ~msqueue();
    void enqueue(int value);
    int dequeue();
};

/* Elimination stack with collision array */
class elimination_stack {
    struct node {
        int value;
        node* next;
        node(int v) : value(v), next(nullptr) {}
    };
    std::atomic<node*> top;
    std::atomic<int> elim_ops[ELIM_SIZE];
    std::atomic<int> elim_vals[ELIM_SIZE];
public:
    elimination_stack() : top(nullptr) {
        for(int i = 0; i < ELIM_SIZE; i++) {
            elim_ops[i] = 0;
            elim_vals[i] = 0;
        }
    }
    ~elimination_stack();
    void push(int value);
    int pop();
};

/* Flat combining stack */
class fc_stack {
    std::stack<int> data;
    std::mutex lock;
    struct slot {
        std::atomic<int> op;
        std::atomic<int> val;
        std::atomic<int> result;
        std::atomic<bool> done;
    } slots[MAX_THREADS];
    int get_slot();
    void combine();
public:
    fc_stack() {
        for(int i = 0; i < MAX_THREADS; i++) {
            slots[i].op = 0;
            slots[i].done = false;
        }
    }
    void push(int value);
    int pop();
};

/* Flat combining queue */
class fc_queue {
    std::queue<int> data;
    std::mutex lock;
    struct slot {
        std::atomic<int> op;
        std::atomic<int> val;
        std::atomic<int> result;
        std::atomic<bool> done;
    } slots[MAX_THREADS];
    int get_slot();
    void combine();
public:
    fc_queue() {
        for(int i = 0; i < MAX_THREADS; i++) {
            slots[i].op = 0;
            slots[i].done = false;
        }
    }
    void enqueue(int value);
    int dequeue();
};

/* Condition variable without spurious wakeups */
struct condvar_no_spurious {
    std::condition_variable cv;
    std::size_t epoch = 0;

    void wait(std::unique_lock<std::mutex>& lock);
    void signal();
    void broadcast();
};

/* Bounded queue using condition variables */
class bounded_queue {
    static const int SIZE = 50;
    int buffer[SIZE];
    int head, tail, count;
    std::mutex lock;
    condvar_no_spurious not_full, not_empty;
public:
    bounded_queue() : head(0), tail(0), count(0) {}
    void enqueue(int value);
    int dequeue();
};

#endif