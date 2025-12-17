/*
 * sgl_queue.cpp
 * Author: Prudhvi Raj Belide
 *
 * Description: Single Global Lock Queue implementation.
 */

#include "containers.h"

/* Enqueue value with mutex protection */
void sgl_queue::enqueue(int value) {
    std::lock_guard<std::mutex> lk(lock);
    data.push(value);
}

/* Dequeue value with mutex protection */
int sgl_queue::dequeue() {
    std::lock_guard<std::mutex> lk(lock);
    if(data.empty()) throw std::runtime_error("empty");
    int v = data.front();
    data.pop();
    return v;
}