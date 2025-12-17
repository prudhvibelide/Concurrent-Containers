/*
 * elimination_stack.cpp
 * Author: Prudhvi Raj Belide
 *
 * Description: Elimination Stack - optimized Treiber stack with elimination array.
 */

#include "containers.h"

/* Destructor: drain and free all nodes */
elimination_stack::~elimination_stack() {
    while(top.load()) {
        node* n = top.load();
        top.store(n->next);
        delete n;
    }
}

/* Push with elimination: try to pair with pop in array before touching stack */
void elimination_stack::push(int value) {
    node* n = new node(value);
    int slot = rand() % ELIM_SIZE;
    
    /* Attempt elimination: look for waiting pop (op=2) */
    int expected = 2;
    if(elim_ops[slot].compare_exchange_strong(expected, 0)) {
        /* Found a pop, deliver value directly */
        elim_vals[slot] = value;
        delete n;
        return;
    }
    
    /* No elimination, fall back to Treiber stack */
    while(true) {
        node* old_top = top.load();
        n->next = old_top;
        if(top.compare_exchange_weak(old_top, n)) return;
    }
}

/* Pop with elimination: try to pair with push in array before touching stack */
int elimination_stack::pop() {
    int slot = rand() % ELIM_SIZE;
    
    /* Attempt elimination: look for waiting push (op=1) */
    int expected = 1;
    if(elim_ops[slot].compare_exchange_strong(expected, 0)) {
        /* Found a push, take value directly */
        return elim_vals[slot].load();
    }
    
    /* No elimination, fall back to Treiber stack */
    while(true) {
        node* old_top = top.load();
        if(!old_top) throw std::runtime_error("empty");
        
        node* next = old_top->next;
        int v = old_top->value;
        if(top.compare_exchange_weak(old_top, next)) return v;
    }
}