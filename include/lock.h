#ifndef LOCK_H
#define LOCK_H

// This enum is shared between C and C++.
typedef enum {
    LOCK_TYPE_PTHREAD_MUTEX,
    LOCK_TYPE_TICKET,
    LOCK_TYPE_MCS,
    LOCK_TYPE_CLH
} lock_type_t;


#ifdef __cplusplus
// --- C++ Path ---
// For C++, lock_t is an alias for the modern Lock class.
class Lock;
typedef Lock lock_t;
#else
// --- C Path ---
// For C, lock_t is the C-style struct with function pointers.
#include <stdbool.h>
typedef struct lock_s lock_t;
#endif


// --- Shared C-Style Factory and Global Functions ---
// The factory function is the single entry point for creating a lock.
// It is callable from both C and C++.
#ifdef __cplusplus
extern "C" {
#endif

lock_t *create_lock_object(lock_type_t type);

void destroy_lock_object(lock_t *lock_obj);

void release_all_locks_held_by_thread(void);

#ifdef __cplusplus
}
#endif


// --- Include Full Language-Specific Definitions ---
// This is done at the end to ensure all forward declarations and
// type aliases are in place.

#ifdef __cplusplus
#include "Lock.hpp"
#else
#include "lock_c_api.h"
#endif


#endif // LOCK_H
