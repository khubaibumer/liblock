# DualLockLibrary

## Overview

The **DualLockLibrary** is a cross-language locking library designed to provide thread synchronization mechanisms across both C and C++ codebases. It offers multiple types of locks implemented using widely used synchronization algorithms, enabling developers to select the locking mechanism most suited to their specific use case.

This library unifies traditional POSIX mutexes with advanced locking schemes, such as Ticket Locks, MCS Locks, and CLH Locks, to cater to various performance, fairness, and scalability needs.

## Key Features

- **Cross-language support**: Works with both C and C++.
- **Flexible lock types**:
    - **PThread Mutex**: Use of standard POSIX mutex locks.
    - **Ticket Locks**: FIFO order ensuring fairness and reducing contention.
    - **MCS (Mellor-Crummey and Scott) Locks**: Scalable queue-based spinlocks for high-throughput systems.
    - **CLH (Craig, Landin, and Hagersten) Locks**: Allocation-free queue-based spinlocks for improved performance on memory-constrained systems.
- **Optimization**:
    - Cache-friendly alignment to avoid false sharing.
    - CPU-specific relax and yield calls to enhance spinlock performance across different architectures (e.g., x86, ARM).
- **Thread-safety**: Designed for multi-threaded environments.

## Supported Architectures

The library supports common CPU architectures including:
- x86/x64
- ARM64 (AArch64)
- Generic architectures (via fallback mechanisms)

## Installation

1. **Clone the Repository**:
```shell script
git clone <repository-url>
   cd liblock
```


2. **Build the Project**:
   Use a CMake-based setup:
```shell script
mkdir build
   cd build
   cmake ..
   make
```


3. **Include in Your Project**:
    - For C: Include `lock.h` and link the compiled library.
    - For C++: Include `ILock.hpp` and link the same library.

## Usage

### C Language Example

To use the different lock types:

```c++
#include "lock.h"
#include "lock_types.h"

int main() {
    lock_t *lock = create_lock_object(LOCK_TYPE_TICKET);

    // Lock and unlock operations
    lock->_lock(lock, __FILE__, __LINE__);
    // Critical section
    lock->unlock(lock);

    // Clean up
    destroy_lock_object(lock);

    return 0;
}
```


### C++ Language Example

To work with the C++ interface:

```c++
#include "ILock.hpp"
#include "lock_types.h"
#include <memory>
#include <thread>

int main() {
    auto lock = createLock(LOCK_TYPE_MCS);

    // Lock and unlock operations
    lock->lock();
    // Critical section
    lock->unlock();

    return 0;
}
```


## Lock Types

### 1. **PThread Mutex**
- Wrapper around `pthread_mutex_t`.
- Ideal for simple use cases where OS-level mutexes are sufficient.

### 2. **Ticket Lock**
- Ensures strict FIFO order.
- Ideal for preventing starvation in highly concurrent environments.

### 3. **MCS Lock**
- Spinlock-based queue lock.
- Reduces contention and is scalable for large numbers of threads.

### 4. **CLH Lock**
- Queue-style spinlock with allocation-free node management.
- Optimized for cache efficiency and reduced contention.

## Advanced Features

- **Thread-local storage**:
    - Dedicated threads avoid contention with thread-local queue nodes.
- **CPU relaxations**:
    - Architecturally optimized relaxations (`_mm_pause`, `yield`) improve spinlock efficiency across different hardware platforms.
- **Fail-safe designs**:
    - Graceful fallback mechanisms are implemented in case of memory allocation failures or invalid configurations.

## Development & Contribution

1. Fork the repository.
2. Implement new features or fix bugs.
3. Run the test suite.
4. Submit a pull request.

## Tests

The library includes several test cases to validate functionality and performance.

Run tests:
```shell script
make test
```


## License

This project is open source and licensed under [MIT License](LICENSE).

---

Feel free to reach out or submit issues for feature requests or bugs. The **DualLockLibrary** team is dedicated to maintaining and improving this library for all developers!