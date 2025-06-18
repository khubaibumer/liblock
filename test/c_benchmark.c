#include <lock.h> // The unified C/C++ header
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_THREADS 20
// #define INCREMENTS_PER_THREAD 1000
#define INCREMENTS_PER_THREAD 1000000

// --- Shared Data ---
long long g_shared_counter = 0;
lock_t* g_lock = NULL;

// --- Worker Thread ---
void* worker(void *arg) {
    (void)arg;
    for (int i = 0; i < INCREMENTS_PER_THREAD; ++i) {
        lock(g_lock); // Use C macro
        g_shared_counter++;
        g_lock->unlock(g_lock);
    }
    return NULL;
}

// --- Utility Functions ---
double get_time_diff(struct timespec* start, struct timespec* end) {
    return (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) / 1e9;
}

const char* lock_type_to_string(lock_type_t type) {
    switch (type) {
        case LOCK_TYPE_PTHREAD_MUTEX: return "Pthread Mutex";
        case LOCK_TYPE_TICKET:        return "Ticket Lock";
        case LOCK_TYPE_MCS:           return "MCS Lock";
        case LOCK_TYPE_CLH:           return "CLH Lock";
        default:                      return "Unknown";
    }
}

// --- Benchmark Runner ---
void run_benchmark(lock_type_t type, int num_threads) {
    pthread_t threads[MAX_THREADS];
    g_shared_counter = 0;
    g_lock = create_lock_object(type);
    if (!g_lock) {
        fprintf(stderr, "Failed to create C lock for benchmark.\n");
        return;
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int i = 0; i < num_threads; ++i) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }
    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double duration = get_time_diff(&start_time, &end_time);
    long long expected = (long long)num_threads * INCREMENTS_PER_THREAD;
    const char* result = (g_shared_counter == expected) ? "SUCCESS" : "FAIL";

    printf("| %-13s | %3d Threads | %8.4f sec | %s |\n",
           lock_type_to_string(type), num_threads, duration, result);

    destroy_lock_object(g_lock);
    g_lock = NULL;
}

int main() {
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores <= 0) num_cores = 8;
    printf("--- C Lock Library Benchmark ---\n");
    printf("Detected %ld logical cores.\n\n", num_cores);

    printf("+---------------+-------------+------------+----------+\n");
    printf("| Lock Type     | Thread Count| Duration   | Result   |\n");
    printf("+---------------+-------------+------------+----------+\n");

    for (int type = LOCK_TYPE_PTHREAD_MUTEX; type <= LOCK_TYPE_CLH; ++type) {
        for (int threads = 1; threads <= num_cores * 2; threads *= 2) {
             if (threads > MAX_THREADS) break;
             run_benchmark((lock_type_t)type, threads);
        }
        printf("+---------------+-------------+------------+----------+\n");
    }
    return 0;
}
