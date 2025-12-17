/*
 * fc_stack.cpp
 * Author: Prudhvi Raj Belide
 *
 * Description: Flat Combining Stack - delegation-based concurrent stack.
 */

#include "containers.h"
#include <thread>

/* Assign unique slot to each thread */
int fc_stack::get_slot() {
    static std::atomic<int> counter(0);
    static thread_local int my_slot = counter.fetch_add(1);
    return my_slot;
}

/* Combiner thread: scan all slots and execute pending operations */
void fc_stack::combine() {
    for(int i = 0; i < MAX_THREADS; i++) {
        int op = slots[i].op.load();
        
        if(op == 1) {
            /* Execute push request */
            data.push(slots[i].val);
            slots[i].done = true;
        } else if(op == 2) {
            /* Execute pop request */
            if(!data.empty()) {
                slots[i].result = data.top();
                data.pop();
            }
            slots[i].done = true;
        }
    }
}

/* Push: post request to slot and wait for combiner */
void fc_stack::push(int value) {
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

/* Pop: post request to slot and wait for combiner */
int fc_stack::pop() {
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