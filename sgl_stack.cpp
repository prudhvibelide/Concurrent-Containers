/*
 * sgl_stack.cpp
 * Author: Prudhvi Raj Belide
 *
 * Description: Single Global Lock Stack implementation.
 */

#include "containers.h"

/* Push value onto stack with mutex protection */
void sgl_stack::push(int value) {
    std::lock_guard<std::mutex> lk(lock);
    data.push(value);
}

/* Pop value from stack with mutex protection */
int sgl_stack::pop() {
    std::lock_guard<std::mutex> lk(lock);
    if(data.empty()) throw std::runtime_error("empty");
    int v = data.top();
    data.pop();
    return v;
}