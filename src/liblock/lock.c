#define _GNU_SOURCE
#include "lock.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h> // For _mm_pause
#endif

#define CACHE_LINE 64

#if defined(__x86_64__) || defined(__i386__)
#define CPU_RELAX() _mm_pause()
#elif defined(__aarch64__)
#define CPU_RELAX() __asm__ __volatile__ ("yield" ::: "memory")
#else
#define CPU_RELAX() sched_yield() // Fallback for other architectures
#endif

// --- Private C Implementation Structs ---
typedef struct __attribute__((aligned(CACHE_LINE))) {
    _Atomic unsigned int now_serving;
    _Atomic unsigned int next_ticket;
} ticket_lock_impl_t;

typedef struct __attribute__((aligned(CACHE_LINE))) mcs_qnode_s {
    struct mcs_qnode_s *next;
    _Atomic(bool) locked;
} mcs_qnode_t;

typedef struct __attribute__((aligned(CACHE_LINE))) clh_qnode_s {
    _Atomic(bool) locked;
} clh_qnode_t;

typedef struct  {
    lock_type_t type;

    union {
        pthread_mutex_t p_mutex;
        ticket_lock_impl_t ticket_lock;
        _Atomic(mcs_qnode_t *) mcs_lock;
        _Atomic(clh_qnode_t *) clh_lock_tail;
    } impl;
} lock_impl_t;

// --- Thread-Local Storage for C ---
typedef struct held_lock_node_s {
    lock_t *lock_obj;
    const char *file;
    int line;
    struct held_lock_node_s *next;
} held_lock_node_t;

static _Thread_local held_lock_node_t *thread_held_locks_head_c = NULL;
static _Thread_local mcs_qnode_t thread_mcs_qnode_c;
// --- FIX: Allocation-free CLH node management ---
static _Thread_local clh_qnode_t clh_node_A_c;
static _Thread_local clh_qnode_t clh_node_B_c;
static _Thread_local clh_qnode_t *my_clh_node_c = NULL;
static _Thread_local clh_qnode_t *next_clh_node_c = NULL;


// --- Function Prototypes for C ---
static void _mutex_lock(lock_t *self, const char *file, int line);

static void _mutex_unlock(lock_t *self);

static void _ticket_lock(lock_t *self, const char *file, int line);

static void _ticket_unlock(lock_t *self);

static void _mcs_lock(lock_t *self, const char *file, int line);

static void _mcs_unlock(lock_t *self);

static void _clh_lock(lock_t *self, const char *file, int line);

static void _clh_unlock(lock_t *self);


// --- List Management for C ---
static void add_to_held_list_c(lock_t *lock, const char *file, int line) {
    held_lock_node_t *n = malloc(sizeof(held_lock_node_t));
    if (!n) exit(1);
    n->lock_obj = lock;
    n->file = file;
    n->line = line;
    n->next = thread_held_locks_head_c;
    thread_held_locks_head_c = n;
}

static void remove_from_held_list_c(lock_t *lock) {
    held_lock_node_t *curr = thread_held_locks_head_c, *prev = NULL;
    while (curr) {
        if (curr->lock_obj == lock) {
            if (prev) prev->next = curr->next;
            else thread_held_locks_head_c = curr->next;
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

// --- Public C API Implementation ---
lock_t *create_lock_object(lock_type_t type) {
    lock_t *obj = malloc(sizeof(lock_t));
    lock_impl_t *pimpl = malloc(sizeof(lock_impl_t));
    if (!obj || !pimpl) {
        free(obj);
        free(pimpl);
        return NULL;
    }

    // Set all function pointers to NULL initially
    memset(obj, 0, sizeof(lock_t));
    obj->pimpl = pimpl;
    pimpl->type = type;

    switch (type) {
        case LOCK_TYPE_PTHREAD_MUTEX:
            pthread_mutex_init(&pimpl->impl.p_mutex, NULL);
            obj->_lock = _mutex_lock;
            obj->unlock = _mutex_unlock;
            break;
        case LOCK_TYPE_TICKET:
            atomic_init(&pimpl->impl.ticket_lock.now_serving, 0);
            atomic_init(&pimpl->impl.ticket_lock.next_ticket, 0);
            obj->_lock = _ticket_lock;
            obj->unlock = _ticket_unlock;
            break;
        case LOCK_TYPE_MCS:
            atomic_init(&pimpl->impl.mcs_lock, NULL);
            obj->_lock = _mcs_lock;
            obj->unlock = _mcs_unlock;
            break;
        case LOCK_TYPE_CLH:
            atomic_init(&pimpl->impl.clh_lock_tail, NULL);
            obj->_lock = _clh_lock;
            obj->unlock = _clh_unlock;
            break;
        default:
            free(obj);
            free(pimpl);
            return NULL;
    }
    obj->_trylock = NULL;
    return obj;
}

void destroy_lock_object(lock_t *lock_obj) {
    if (!lock_obj) return;
    lock_impl_t *pimpl = (lock_impl_t *) lock_obj->pimpl;
    if (pimpl && pimpl->type == LOCK_TYPE_PTHREAD_MUTEX) {
        pthread_mutex_destroy(&pimpl->impl.p_mutex);
    }
    free(pimpl);
    free(lock_obj);
}

void release_all_locks_held_by_thread(void) {
    while (thread_held_locks_head_c) {
        thread_held_locks_head_c->lock_obj->unlock(thread_held_locks_head_c->lock_obj);
    }
}

// --- C Implementations ---
static void _mutex_lock(lock_t *self, const char *f, int l) {
    (void) f;
    (void) l;
    lock_impl_t *p = self->pimpl;
    pthread_mutex_lock(&p->impl.p_mutex);
}

static void _mutex_unlock(lock_t *self) {
    lock_impl_t *p = self->pimpl;
    pthread_mutex_unlock(&p->impl.p_mutex);
}

static void _ticket_lock(lock_t *self, const char *f, int l) {
    (void) f;
    (void) l;
    lock_impl_t *p = self->pimpl;
    unsigned int t = atomic_fetch_add_explicit(&p->impl.ticket_lock.next_ticket, 1, memory_order_relaxed);
    while (atomic_load_explicit(&p->impl.ticket_lock.now_serving, memory_order_acquire) != t) { sched_yield(); }
}

static void _ticket_unlock(lock_t *self) {
    lock_impl_t *p = self->pimpl;
    atomic_fetch_add_explicit(&p->impl.ticket_lock.now_serving, 1, memory_order_release);
}

static void _mcs_lock(lock_t *self, const char *f, int l) {
    (void) f;
    (void) l;
    lock_impl_t *p = self->pimpl;
    thread_mcs_qnode_c.next = NULL;
    atomic_store_explicit(&thread_mcs_qnode_c.locked, true, memory_order_relaxed);
    mcs_qnode_t *pred = atomic_exchange_explicit(&p->impl.mcs_lock, &thread_mcs_qnode_c, memory_order_acq_rel);
    if (pred) {
        atomic_store_explicit(&pred->next, &thread_mcs_qnode_c, memory_order_release);
        while (atomic_load_explicit(&thread_mcs_qnode_c.locked, memory_order_acquire)) sched_yield();
    }
}

static void _mcs_unlock(lock_t *self) {
    lock_impl_t *p = self->pimpl;
    mcs_qnode_t *succ = atomic_load_explicit(&thread_mcs_qnode_c.next, memory_order_acquire);
    if (!succ) {
        mcs_qnode_t *me = &thread_mcs_qnode_c;
        if (atomic_compare_exchange_strong_explicit(&p->impl.mcs_lock, &me, NULL, memory_order_release,
                                                    memory_order_relaxed)) return;
        while (!(succ = atomic_load_explicit(&thread_mcs_qnode_c.next, memory_order_acquire))) sched_yield();
    }
    atomic_store_explicit(&succ->locked, false, memory_order_release);
}

// --- CORRECTED CLH IMPLEMENTATION ---
static void _clh_lock(lock_t *self, const char *f, int l) {
    (void) f;
    (void) l;
    lock_impl_t *p = self->pimpl;

    // One-time initialization of thread-local pointers at runtime
    if (my_clh_node_c == NULL) {
        my_clh_node_c = &clh_node_A_c;
        next_clh_node_c = &clh_node_B_c;
    }

    // Swap nodes to prevent race condition on immediate re-lock by the same thread
    clh_qnode_t *temp = my_clh_node_c;
    my_clh_node_c = next_clh_node_c;
    next_clh_node_c = temp;

    atomic_store_explicit(&my_clh_node_c->locked, true, memory_order_relaxed);
    clh_qnode_t *pred = atomic_exchange_explicit(&p->impl.clh_lock_tail, my_clh_node_c, memory_order_acq_rel);
    if (pred) {
        while (atomic_load_explicit(&pred->locked, memory_order_acquire)) {
            sched_yield();
        }
    }
}

static void _clh_unlock(lock_t *self) {
    (void) self;
    // Unlock the node that our successor is spinning on. No deallocation needed.
    if (my_clh_node_c) {
        atomic_store_explicit(&my_clh_node_c->locked, false, memory_order_release);
    }
}
