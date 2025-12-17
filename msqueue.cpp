/*
 * msqueue.cpp
 * Author: Prudhvi Raj Belide
 *
 * Description: Michael & Scott Queue - lock-free FIFO queue with dummy node.
 */

#include "containers.h"

/* Initialize with dummy node to simplify empty queue handling */
msqueue::msqueue() {
    node* dummy = new node(0);
    head.store(dummy);
    tail.store(dummy);
}

/* Destructor: drain and free all nodes */
msqueue::~msqueue() {
    while(head.load() != tail.load()) {
        node* n = head.load();
        head.store(n->next);
        delete n;
    }
    delete head.load();
}

/* Lock-free enqueue with helping mechanism */
void msqueue::enqueue(int value) {
    node* n = new node(value);
    
    while(true) {
        node* last = tail.load();
        node* next = last->next.load();
        
        /* Verify tail hasn't changed */
        if(last == tail.load()) {
            if(!next) {
                /* Tail is at actual end, try to link new node */
                if(last->next.compare_exchange_weak(next, n)) {
                    /* Try to swing tail forward (can fail, another thread will help) */
                    tail.compare_exchange_weak(last, n);
                    return;
                }
            } else {
                /* Tail is lagging, help advance it */
                tail.compare_exchange_weak(last, next);
            }
        }
    }
}

/* Lock-free dequeue with helping mechanism
   Note: leaks first node (no safe reclamation) */
int msqueue::dequeue() {
    while(true) {
        node* first = head.load();
        node* last = tail.load();
        node* next = first->next.load();
        
        /* Verify head hasn't changed */
        if(first == head.load()) {
            if(first == last) {
                /* Queue appears empty or tail lagging */
                if(!next) throw std::runtime_error("empty");
                
                /* Help advance tail */
                tail.compare_exchange_weak(last, next);
            } else {
                /* Queue has items, read value and try to advance head */
                int v = next->value;
                if(head.compare_exchange_weak(first, next)) {
                    return v;
                }
            }
        }
    }
}