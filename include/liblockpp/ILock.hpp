#ifndef LIBLOCKPP_H
#define LIBLOCKPP_H

#include "lock_types.h"
#include <memory>

/**
 * @brief Defines the public C++ interface for all lock types.
 *
 * This class uses pure virtual functions, making it an abstract base class.
 * Concrete lock implementations will inherit from ILock and provide
 * implementations for these methods.
 */
class ILock {
public:
    /**
     * @brief Virtual destructor.
     * Ensures that derived class destructors are called correctly.
     */
    virtual ~ILock() = default;

    /**
     * @brief Acquires the lock, blocking if necessary.
     */
    virtual void lock() = 0;

    /**
     * @brief Releases the lock.
     */
    virtual void unlock() = 0;

    /**
     * @brief Attempts to acquire the lock without blocking.
     * @return true if the lock was acquired, false otherwise.
     */
    virtual bool trylock() = 0;
};

/**
 * @brief Factory function to create a lock object of a specific type.
 *
 * This is the single entry point for creating any lock from the C++ library.
 * It returns a smart pointer to the ILock interface, hiding the concrete
 * implementation type from the user.
 *
 * @param type The underlying lock mechanism to use.
 * @return A std::unique_ptr to a new ILock object.
 * @throws std::runtime_error if an unknown lock type is requested.
 */
std::unique_ptr<ILock> createLock(lock_type_t type);

#endif // LIBLOCKPP_H
