#ifndef LOCK_C_API_H
#define LOCK_C_API_H

// This header is included by the main "lock.h" when compiling C code.
// It defines the C-style vtable struct that C consumers will interact with.

struct lock_s {
    /**
     * @brief Acquires the lock (private, use the 'lock' macro).
     */
    void (*_lock)(lock_t *self, const char *file, int line);

    /**
     * @brief Releases the lock.
     */
    void (*unlock)(lock_t *self);

    /**
     * @brief Tries to acquire the lock (private, use the 'trylock' macro).
     */
    bool (*_trylock)(lock_t *self, const char *file, int line);

    /**
     * @brief Pointer to the private, internal implementation details.
     */
    void *pimpl;
};

// Convenience macros for the C API to automatically pass file and line info.
#define lock(lock_ptr) (lock_ptr)->_lock((lock_ptr), __FILE__, __LINE__)
#define trylock(lock_ptr) (lock_ptr)->_trylock((lock_ptr), __FILE__, __LINE__)

#endif // LOCK_C_API_H
