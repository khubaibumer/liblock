#ifndef LOCK_HPP
#define LOCK_HPP

#include "lock_types.h"

#include <memory>

// The unified "lock.h" header includes this file when compiling C++.
// This file only needs to define the C++ Lock class.

class Lock {
public:
    // The public API is clean and object-oriented.
    void lock();
    void unlock();
    bool trylock();

    // The factory function is the only way to create a lock.
    // The constructor is private.
    friend Lock* create_lock_object(lock_type_t type);

    // Make the class non-copyable but movable
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    Lock(Lock&&) noexcept;
    Lock& operator=(Lock&&) noexcept;
    ~Lock();

private:
    explicit Lock(lock_type_t type); // Private constructor

    // Using the PIMPL idiom to hide implementation details
    class LockImpl;
    std::unique_ptr<LockImpl> pimpl;
};

#endif // LOCK_HPP
