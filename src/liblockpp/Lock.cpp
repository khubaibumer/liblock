#include "ILock.hpp"
#include "lock_types.h"
#include <mutex>
#include <atomic>
#include <thread>
#include <stdexcept>
#include <new>
#include <utility> // For std::swap

namespace { // Anonymous namespace to hide concrete implementation classes

// --- Mutex Lock Implementation (Correct) ---
class MutexLock final : public ILock {
public:
    void lock() override { _mutex.lock(); }
    void unlock() override { _mutex.unlock(); }
    bool trylock() override { return _mutex.try_lock(); }
private:
    std::mutex _mutex;
};

// --- Ticket Lock Implementation (Correct) ---
class TicketLock final : public ILock {
public:
    TicketLock() : _now_serving(0), _next_ticket(0) {}
    void lock() override {
        const auto my_ticket = _next_ticket.fetch_add(1, std::memory_order_relaxed);
        while (_now_serving.load(std::memory_order_acquire) != my_ticket) {
            std::this_thread::yield();
        }
    }
    void unlock() override {
        const auto next_to_serve = _now_serving.load(std::memory_order_relaxed) + 1;
        _now_serving.store(next_to_serve, std::memory_order_release);
    }
    bool trylock() override {
        unsigned int current = _now_serving.load(std::memory_order_relaxed);
        return _next_ticket.compare_exchange_strong(current, current + 1, std::memory_order_acquire, std::memory_order_relaxed);
    }
private:
    std::atomic<unsigned int> _now_serving;
    std::atomic<unsigned int> _next_ticket;
};

// --- MCS Lock Implementation (Deadlock Corrected) ---
struct mcs_qnode_cpp { std::atomic<mcs_qnode_cpp*> next = nullptr; std::atomic<bool> locked = false; };
class MCSLock final : public ILock {
public:
    void lock() override {
        thread_local mcs_qnode_cpp node;
        node.next.store(nullptr, std::memory_order_relaxed);
        node.locked.store(true, std::memory_order_relaxed);
        auto* const pred = _tail.exchange(&node, std::memory_order_acq_rel);
        if (pred) {
            pred->next.store(&node, std::memory_order_release);
            while (node.locked.load(std::memory_order_acquire)) { std::this_thread::yield(); }
        }
    }
    void unlock() override {
        thread_local mcs_qnode_cpp node;
        mcs_qnode_cpp* succ = node.next.load(std::memory_order_acquire);

        if (succ == nullptr) {
            // I appear to be the last in line.
            mcs_qnode_cpp* me = &node;
            // Try to set the tail from me to nullptr.
            if (_tail.compare_exchange_strong(me, nullptr, std::memory_order_release, std::memory_order_relaxed)) {
                // Succeeded, nobody came after me. We are done.
                return;
            }
            // If CAS failed, it means a successor just arrived. Spin until we see it.
            while ((succ = node.next.load(std::memory_order_acquire)) == nullptr) {
                std::this_thread::yield();
            }
        }

        // Pass the lock to the successor.
        succ->locked.store(false, std::memory_order_release);
    }
    bool trylock() override { return false; /* Not implemented for queuing locks */ }
private:
    std::atomic<mcs_qnode_cpp*> _tail = nullptr;
};

// --- CLH Lock Implementation (Allocation-Free) ---
struct clh_qnode_cpp { std::atomic<bool> locked = false; };
class CLHLock final : public ILock {
public:
    void lock() override {
        std::swap(_my_node, _next_node);
        _my_node->locked.store(true, std::memory_order_relaxed);
        clh_qnode_cpp* pred = _tail.exchange(_my_node, std::memory_order_acq_rel);
        if (pred) {
            while (pred->locked.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }
    }
    void unlock() override {
        _my_node->locked.store(false, std::memory_order_release);
    }
    bool trylock() override {
        clh_qnode_cpp* current_tail = _tail.load(std::memory_order_relaxed);
        return (current_tail == nullptr || !current_tail->locked.load(std::memory_order_relaxed));
    }
private:
    std::atomic<clh_qnode_cpp*> _tail = nullptr;
    thread_local static inline clh_qnode_cpp _node_A;
    thread_local static inline clh_qnode_cpp _node_B;
    thread_local static inline clh_qnode_cpp* _my_node = &_node_A;
    thread_local static inline clh_qnode_cpp* _next_node = &_node_B;
};

} // end anonymous namespace

// --- Public Factory Function Implementation ---
std::unique_ptr<ILock> createLock(lock_type_t type) {
    switch (type) {
        case LOCK_TYPE_PTHREAD_MUTEX: return std::make_unique<MutexLock>();
        case LOCK_TYPE_TICKET:        return std::make_unique<TicketLock>();
        case LOCK_TYPE_MCS:           return std::make_unique<MCSLock>();
        case LOCK_TYPE_CLH:           return std::make_unique<CLHLock>();
        default:                      throw std::runtime_error("Unknown lock type requested.");
    }
}
