#ifndef LOCK_TYPES_H
#define LOCK_TYPES_H

// This enum is shared between C and C++ libraries.
typedef enum {
    LOCK_TYPE_PTHREAD_MUTEX,
    LOCK_TYPE_TICKET,
    LOCK_TYPE_MCS,
    LOCK_TYPE_CLH
} lock_type_t;

#endif // LOCK_TYPES_H
