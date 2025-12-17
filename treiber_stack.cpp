/*
 * treiber_stack.cpp
 * Author: Prudhvi Raj Belide
 *
 * Description: Treiber Stack - lock-free stack using CAS operations.
 */

#include "containers.h"

/* Destructor: drain and free all nodes */
treiber_stack::~treiber_stack() {
    while(top.load()) {
        node* n = top.load();
        top.store(n->next);
        delete n;
    }
}

/* Lock-free push using compare-and-swap */
void treiber_stack::push(int value) {
    node* n = new node(value);
    while(true) {
        node* old_top = top.load();
        n->next = old_top;
        
        /* Try to swing top pointer to new node */
        if(top.compare_exchange_weak(old_top, n)) return;
    }
}

/* Lock-free pop using compare-and-swap
   Note: leaks old_top node (no safe reclamation) */
int treiber_stack::pop() {
    while(true) {
        node* old_top = top.load();
        if(!old_top) throw std::runtime_error("empty");
        
        node* next = old_top->next;
        int v = old_top->value;
        
        /* Try to advance top to next node */
        if(top.compare_exchange_weak(old_top, next)) {
            return v;
        }
    }
}