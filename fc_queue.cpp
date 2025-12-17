/*
 * fc_queue.cpp
 * Author: Prudhvi Raj Belide
 *
 * Description: Flat Combining Queue - delegation-based concurrent queue.
 */

#include "containers.h"
#include <thread>

/* Assign unique slot to each thread */
int fc_queue::get_slot() {
    static thread_local int my_slot = -1;
    if(my_slot == -1) {
        static std::atomic<int> counter(0);
        my_slot = counter.fetch_add(1) % MAX_THREADS;
    }
    return my_slot;
}

/* Combiner thread: scan all slots and execute pending operations */
void fc_queue::combine() {
    for(int i = 0; i < MAX_THREADS; i++) {
        int op = slots[i].op.load();
        
        if(op == 1) {
            /* Execute enqueue request */
            data.push(slots[i].val);
            slots[i].done = true;
        } else if(op == 2) {
            /* Execute dequeue request */
            if(!data.empty()) {
                slots[i].result = data.front();
                data.pop();
            }
            slots[i].done = true;
        }
    }
}

/* Enqueue: post request to slot and wait for combiner */
void fc_queue::enqueue(int value) {
    int s = get_slot();
    slots[s].op = 1;
    slots[s].val = value;
    slots[s].done = false;
    
    /* Try to become combiner, otherwise wait */
    if(lock.try_lock()) {
        combine();
        lock.unlock();
    } else {
        while(!slots[s].done) std::this_thread::yield();
    }
    
    slots[s].op = 0;
}

/* Dequeue: post request to slot and wait for combiner */
int fc_queue::dequeue() {
    int s = get_slot();
    slots[s].op = 2;
    slots[s].done = false;
    
    /* Try to become combiner, otherwise wait */
    if(lock.try_lock()) {
        combine();
        lock.unlock();
    } else {
        while(!slots[s].done) std::this_thread::yield();
    }
    
    slots[s].op = 0;
    if(slots[s].result == -1) throw std::runtime_error("empty");
    return slots[s].result;
}