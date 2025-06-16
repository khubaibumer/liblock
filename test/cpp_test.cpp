#include <ILock.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <numeric>

#define NUM_THREADS 4
#define INCREMENTS 100000

std::unique_ptr<ILock> g_lock;
int g_counter = 0;

void worker() {
    for (int i = 0; i < INCREMENTS; ++i) {
        g_lock->lock();
        g_counter++;
        g_lock->unlock();
    }
}

int main() {
    std::cout << "--- C++ Library Test ---" << std::endl;
    try {
        g_lock = createLock(LOCK_TYPE_TICKET);
    } catch (const std::exception &e) {
        std::cerr << "Failed to create lock: " << e.what() << std::endl;
        return 1;
    }

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker);
    }

    for (auto &t: threads) {
        t.join();
    }

    std::cout << "Final counter value: " << g_counter << " (Expected: " << NUM_THREADS * INCREMENTS << ")" << std::endl;
    std::cout << "Test finished." << std::endl;
    // g_lock is automatically destroyed by unique_ptr
    return 0;
}
