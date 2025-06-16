#include <ILock.hpp> // C++ programs should prefer including the specific interface
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <memory>
#include <iomanip>
#include <numeric>

#define MAX_THREADS 20
#define INCREMENTS_PER_THREAD 1000000

// --- Shared Data ---
long long g_shared_counter = 0;
std::unique_ptr<ILock> g_lock;

// --- Worker Thread ---
void worker() {
    for (int i = 0; i < INCREMENTS_PER_THREAD; ++i) {
        g_lock->lock();
        g_shared_counter++;
        g_lock->unlock();
    }
}

// --- Utility Functions ---
const char* lock_type_to_string(lock_type_t type) {
    switch (type) {
        case LOCK_TYPE_PTHREAD_MUTEX: return "std::mutex";
        case LOCK_TYPE_TICKET:        return "Ticket Lock";
        case LOCK_TYPE_MCS:           return "MCS Lock";
        case LOCK_TYPE_CLH:           return "CLH Lock";
        default:                      return "Unknown";
    }
}

// --- Benchmark Runner ---
void run_benchmark(lock_type_t type, int num_threads) {
    g_shared_counter = 0;
    try {
        g_lock = createLock(type);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create C++ lock: " << e.what() << std::endl;
        return;
    }

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    long long expected = static_cast<long long>(num_threads) * INCREMENTS_PER_THREAD;
    const char* result = (g_shared_counter == expected) ? "SUCCESS" : "FAIL";

    std::cout << "| " << std::left << std::setw(13) << lock_type_to_string(type)
              << " | " << std::right << std::setw(3) << num_threads << " Threads"
              << " | " << std::fixed << std::setprecision(4) << std::setw(8) << duration.count() << " sec"
              << " | " << result << " |" << std::endl;
    // g_lock is automatically destroyed by unique_ptr
}

int main() {
    unsigned int num_cores = std::thread::hardware_concurrency();
    if (num_cores == 0) num_cores = 8;
    std::cout << "--- C++ Lock Library Benchmark ---\n";
    std::cout << "Detected " << num_cores << " logical cores.\n\n";

    std::cout << "+---------------+-------------+------------+----------+" << std::endl;
    std::cout << "| Lock Type     | Thread Count| Duration   | Result   |" << std::endl;
    std::cout << "+---------------+-------------+------------+----------+" << std::endl;

    for (int type = LOCK_TYPE_PTHREAD_MUTEX; type <= LOCK_TYPE_CLH; ++type) {
        for (unsigned int threads = 1; threads <= num_cores * 2; threads *= 2) {
             if (threads > MAX_THREADS) break;
             run_benchmark(static_cast<lock_type_t>(type), threads);
        }
        std::cout << "+---------------+-------------+------------+----------+" << std::endl;
    }
    return 0;
}
