#include <lock.h>
#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS 4
#define INCREMENTS 100000

lock_t *g_lock;
int g_counter = 0;

void *worker(void *arg) {
    (void) arg;
    for (int i = 0; i < INCREMENTS; ++i) {
        lock(g_lock); // Use macro
        g_counter++;
        g_lock->unlock(g_lock);
    }
    return NULL;
}

int main() {
    printf("--- C Library Test ---\n");
    g_lock = create_lock_object(LOCK_TYPE_TICKET);
    if (!g_lock) {
        fprintf(stderr, "Failed to create lock\n");
        return 1;
    }

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    printf("Final counter value: %d (Expected: %d)\n", g_counter, NUM_THREADS * INCREMENTS);

    destroy_lock_object(g_lock);
    printf("Test finished.\n");
    return 0;
}
