/*
 * condvar.cpp
 * Author: Prudhvi Raj Belide
 *
 * Description: Condition Variable without spurious wakeups (Waking up without being notified).
 */

#include "containers.h"


/* 
1. Queue full → Producer waits
2. Consumer takes item → signals "not_full"
3. Epoch changes → Producer wakes up
4. Producer adds item → signals "not_empty"
5. Loop continues
*/

/*Epoch = generation number
Starts at 0
Each signal increments it
Threads remember their epoch when they wait*/

/* Thread saves current epoch value (say 5)
Calls standard cv.wait() with condition
Standard CV might wake spuriously
Checks: epoch != my_epoch → if still 5, go back to sleep
Only proceeds when epoch ≠ 5 (someone called signal)
*/

//Wait does 3 things : Release the lock, Put thread to sleep, When woken, reacquire lock
void condvar_no_spurious::wait(std::unique_lock<std::mutex>& lock) {
    std::size_t my_epoch = epoch; //Save current epoch 
    cv.wait(lock, [&] { return epoch != my_epoch; }); //Sleep until epoch changes
}

void condvar_no_spurious::signal() {
    ++epoch;
    cv.notify_one(); // Wake one thread
}

void condvar_no_spurious::broadcast() {
    ++epoch;
    cv.notify_all();
}

//ADD ITEM
void bounded_queue::enqueue(int v) { //Add item to queue
    std::unique_lock<std::mutex> lk(lock); //Lock the queue
    while(count == SIZE)  //Check if queue is full
        not_full.wait(lk); //Wait until not full

    buffer[tail] = v;  //add value to buffer
    tail = (tail + 1) % SIZE; //Move tail forward (circular buffer)
    count++;
    not_empty.signal(); //Wake one consumer saying item exists
}

int bounded_queue::dequeue() {
    std::unique_lock<std::mutex> lk(lock);
    while(count == 0)
        not_empty.wait(lk);

    int v = buffer[head];
    head = (head + 1) % SIZE;
    count--;

    not_full.signal(); //Wake one producer
    return v;
}
