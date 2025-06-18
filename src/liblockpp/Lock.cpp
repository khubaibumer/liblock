#include "ILock.hpp"
#include "lock_types.h"
#include <mutex>
#include <atomic>
#include <thread>
#include <stdexcept>
#ifdef __GNUC__
#include <immintrin.h> // For _mm_pause on x86/x64
#endif

#if __cplusplus >= 201703L
#define CACHE_ALIGN alignas(std::hardware_destructive_interference_size)
#else
#define CACHE_ALIGN alignas(64)
#endif


namespace {
    // Anonymous namespace to hide concrete implementation classes

    inline void cpu_relax() {
#if defined(__GNUC__) || defined(__clang__)
        // For x86/x64
#if defined(__x86_64__) || defined(__i386__)
        _mm_pause();
#elif defined(__aarch64__)
        __asm__ __volatile__ ("yield" ::: "memory");
#else
        std::this_thread::yield(); // Fallback for other architectures
#endif
#else
        std::this_thread::yield(); // Generic fallback
#endif
    }

    // --- Mutex Lock Implementation ---
    class MutexLock final : public ILock {
    public:
        void lock() override { _mutex.lock(); }
        void unlock() override { _mutex.unlock(); }
        bool trylock() override { return _mutex.try_lock(); }

    private:
        std::mutex _mutex;
    };

    // --- Ticket Lock Implementation ---
    class TicketLock final : public ILock {
    public:
        TicketLock() : _now_serving(0), _next_ticket(0) {
        }

        void lock() override {
            const auto my_ticket = _next_ticket.fetch_add(1, std::memory_order_relaxed);
            while (_now_serving.load(std::memory_order_acquire) != my_ticket) {
                cpu_relax();
            }
        }

        void unlock() override {
            const auto next_to_serve = _now_serving.load(std::memory_order_relaxed) + 1;
            _now_serving.store(next_to_serve, std::memory_order_release);
        }

        bool trylock() override {
            unsigned int current_serving = _now_serving.load(std::memory_order_relaxed);
            unsigned int next_expected_ticket = current_serving; // The ticket we'd expect to take
            // Only try to acquire a ticket if _next_ticket is currently what's being served.
            // This prevents taking a ticket that's too far ahead if multiple threads are trylocking.
            if (_next_ticket.compare_exchange_strong(next_expected_ticket, next_expected_ticket + 1,
                                                     std::memory_order_acquire, std::memory_order_relaxed)) {
                return true; // Successfully acquired the ticket, and it's our turn
            }
            return false; // Failed to acquire or not our turn
        }

    private:
        CACHE_ALIGN std::atomic<unsigned int> _now_serving;
        CACHE_ALIGN std::atomic<unsigned int> _next_ticket;
    };

    // --- MCS Lock Implementation (Deadlock Corrected) ---
    struct CACHE_ALIGN mcs_qnode_cpp {
        std::atomic<mcs_qnode_cpp *> next = nullptr;
        std::atomic<bool> locked = false;
    };

    class MCSLock final : public ILock {
    public:
        void lock() override {
            _node.next.store(nullptr, std::memory_order_relaxed);
            _node.locked.store(true, std::memory_order_relaxed);
            auto *const pred = _tail.exchange(&_node, std::memory_order_acq_rel);
            if (pred) {
                // Ensure the pred->next store is visible before the current thread spins.
                // release ensures visibility of _node's state to pred.
                pred->next.store(&_node, std::memory_order_release);
                while (_node.locked.load(std::memory_order_acquire)) {
                    cpu_relax(); // Use CPU-specific pause
                }
            }
        }

        void unlock() override {
            mcs_qnode_cpp *succ = _node.next.load(std::memory_order_acquire);

            if (succ == nullptr) {
                mcs_qnode_cpp *me = &_node;
                // Try to swing tail to nullptr. If it fails, another thread has already
                // put itself on the queue.
                if (_tail.compare_exchange_strong(me, nullptr, std::memory_order_release, std::memory_order_relaxed)) {
                    return; // Successfully unlocked and no successor
                }
                // We lost the race to clear the tail, so a successor exists.
                // Spin until that successor has updated our node's next pointer.
                while ((succ = _node.next.load(std::memory_order_acquire)) == nullptr) {
                    cpu_relax(); // Use CPU-specific pause
                }
            }
            // Hand off the lock to the successor
            succ->locked.store(false, std::memory_order_release);
        }

        bool trylock() override {
            return false; /* Not implemented */
        }

    private:
        CACHE_ALIGN std::atomic<mcs_qnode_cpp *> _tail = nullptr;
        CACHE_ALIGN thread_local static inline mcs_qnode_cpp _node;
    };

    // --- CLH Lock Implementation (Allocation-Free) ---
    struct CACHE_ALIGN clh_qnode_cpp {
        std::atomic<bool> locked = false;
    };

    class CLHLock final : public ILock {
    public:
        void lock() override {
            // Swap nodes. _my_node becomes the node for this acquire, _next_node becomes the one for the next acquire.
            // This is a correct approach for the allocation-free CLH.
            std::swap(_my_node_ptr, _next_node_ptr);
            _my_node_ptr->locked.store(true, std::memory_order_relaxed); // Current node is now 'locked'

            // Atomically set _tail to _my_node_ptr and get the previous tail (our predecessor)
            clh_qnode_cpp *pred = _tail.exchange(_my_node_ptr, std::memory_order_acq_rel);

            if (pred) {
                // If there's a predecessor, spin until its 'locked' flag becomes false
                while (pred->locked.load(std::memory_order_acquire)) {
                    cpu_relax(); // Use CPU-specific pause
                }
            }
            // If pred is nullptr, we are the first in the queue.
            // If pred->locked is false, predecessor has released the lock.
        }

        void unlock() override {
            // Atomically set _my_node's locked flag to false, releasing the lock for the successor.
            _my_node_ptr->locked.store(false, std::memory_order_release);
        }

        bool trylock() override {
            clh_qnode_cpp *expected_tail = _tail.load(std::memory_order_relaxed);

            // Early exit: If the queue tail exists and it's locked, the lock is currently held.
            // No need to try to enqueue. `acquire` ensures we see the latest 'locked' status.
            if (expected_tail != nullptr && expected_tail->locked.load(std::memory_order_acquire)) {
                return false; // Lock is busy
            }

            // If we reach here, the lock *appears* free (queue is empty, or tail is unlocked).
            // Now, try to acquire it using a Compare-And-Swap (CAS).
            // We use `_next_node_ptr` as the node this thread will attempt to use for this acquisition.
            clh_qnode_cpp *node_for_attempt = _next_node_ptr;
            // Optimistically set its 'locked' flag to true.
            node_for_attempt->locked.store(true, std::memory_order_relaxed);

            // Attempt to make our `node_for_attempt` the new tail.
            // `expected_tail` is the value _tail *should* be right now for the CAS to succeed.
            // If it matches, _tail is updated to `node_for_attempt`.
            // If it doesn't match, another thread won the race, and `expected_tail` is updated to the actual current tail.
            if (_tail.compare_exchange_strong(expected_tail, node_for_attempt,
                                              std::memory_order_acq_rel, std::memory_order_relaxed)) {
                // CAS succeeded: We are now the tail. Lock acquired.
                // Now, crucial for the allocation-free model:
                // We successfully used `_next_node_ptr` to acquire the lock.
                // So, we swap the roles of `_my_node_ptr` and `_next_node_ptr`.
                // This makes `node_for_attempt` (which was `_next_node_ptr`) the new `_my_node_ptr`
                // so that subsequent `unlock()` calls use the correct node.
                // `_next_node_ptr` now points to the other `thread_local` node, ready for the next acquisition attempt.
                std::swap(_my_node_ptr, _next_node_ptr);
                return true; // Lock acquired
            }
            // CAS failed: Another thread beat us to setting the tail.
            // We did not acquire the lock. Our `node_for_attempt` is NOT linked into the queue.
            // No cleanup or undo is needed.
            // The `_my_node_ptr` and `_next_node_ptr` remain in their original states from before this `trylock` call.
            return false; // Lock not acquired
        }

    private:
        CACHE_ALIGN std::atomic<clh_qnode_cpp *> _tail = nullptr;

        // Ensure nodes are aligned to cache lines to prevent false sharing.
        // C++17 `std::hardware_destructive_interference_size` is preferred.
        CACHE_ALIGN thread_local static inline clh_qnode_cpp _node_A;
        CACHE_ALIGN thread_local static inline clh_qnode_cpp _node_B;

        // Use _my_node_ptr and _next_node_ptr for clarity and consistency with the original code's intent
        thread_local static inline clh_qnode_cpp *_my_node_ptr = &_node_A;
        thread_local static inline clh_qnode_cpp *_next_node_ptr = &_node_B;
    };
} // end anonymous namespace

// --- Public Factory Function Implementation ---
std::unique_ptr<ILock> createLock(lock_type_t type) {
    switch (type) {
        case LOCK_TYPE_PTHREAD_MUTEX: return std::make_unique<MutexLock>();
        case LOCK_TYPE_TICKET: return std::make_unique<TicketLock>();
        case LOCK_TYPE_MCS: return std::make_unique<MCSLock>();
        case LOCK_TYPE_CLH: return std::make_unique<CLHLock>();
        default: throw std::runtime_error("Unknown lock type requested.");
    }
}
