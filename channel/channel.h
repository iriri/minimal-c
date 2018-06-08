/* channel.h v0.0.0
 * Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file.
 *
 * The semantics of this library are inspired by channels from the Go
 * programming language.
 *
 * The implementations of sending, receiving, and selection are based on
 * algorithms described in "Go channels on steroids" by Dmitry Vyukov. */
#ifndef CHANNEL_H
#define CHANNEL_H
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef _POSIX_THREADS
#include <pthread.h>
#if _POSIX_SEMAPHORES >= 200112l
#include <errno.h>
#include <semaphore.h>
#include <time.h>
#elif defined __APPLE__
#include <dispatch/dispatch.h>
#else
#error "Semaphore implementation with timeout support missing."
#endif
#else
#error "Target does not support POSIX threads."
#endif

/* Assumptions */
#if ATOMIC_LLONG_LOCK_FREE != 2
#error "Not sure if this works if `_Atomic uint64_t` isn't lock-free."
#elif !defined(__BYTE_ORDER__) || (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error "The changes for big endian are trivial but I need a box to test on."
#endif

/* ------------------------------- Interface ------------------------------- */
#define CHANNEL_H_VERSION 0l // 0.0.0

typedef union channel channel;
typedef struct channel_set channel_set;

/* Return codes */
typedef enum channel_rc {
    CH_OK,
    CH_WBLOCK,
    CH_CLOSED,
} channel_rc;

#define CH_SEL_CLOSED UINT32_MAX // Should be merged somehow with CH_CLOSED?
#define CH_SEL_WBLOCK (UINT32_MAX - 1) // Same

/* Op codes */
typedef enum channel_op {
    CH_NOOP,
    CH_SEND,
    CH_RECV,
} channel_op;

/* Exported "functions" */
#define ch_make(T, cap) channel_make(sizeof(T), cap)
#define ch_drop(c) channel_drop(c)
#define ch_dup(c) channel_dup(c)
#define ch_close(c) channel_close(c)

#define ch_send(c, elt) channel_send(c, &(elt), sizeof(elt))
#define ch_trysend(c, elt) channel_trysend(c, &(elt), sizeof(elt))
#define ch_timedsend(c, elt, timeout) \
    channel_timedsend(c, &(elt), timeout, sizeof(elt))
#define ch_forcesend(c, elt) channel_forcesend(c, &(elt), sizeof(elt))

#define ch_recv(c, elt) channel_recv(c, &(elt), sizeof(elt))
#define ch_tryrecv(c, elt) channel_tryrecv(c, &(elt), sizeof(elt))
#define ch_timedrecv(c, elt, timeout) \
    channel_timedrecv(c, &(elt), timeout, sizeof(elt))

#define ch_set_make(cap) channel_set_make(cap)
#define ch_set_drop(s) channel_set_drop(s)
#define ch_set_add(s, c, op, elt) \
    channel_set_add(s, c, op, &(elt), sizeof(elt))
#define ch_set_rereg(s, id, op, elt) \
    channel_set_rereg(s, id, op, &(elt), sizeof(elt))
#define ch_select(s, timeout) channel_select(s, timeout)

/* These declarations must be present in exactly one compilation unit. */
#define CHANNEL_EXTERN_DECL \
    CHANNEL_SEM_WAIT_DECL_ \
    CHANNEL_SEM_TIMEDWAIT_DECL_ \
    extern inline void channel_assert_( \
        const char *, unsigned, const char *) __attribute__((noreturn)); \
    extern inline channel *channel_make(size_t, uint32_t); \
    extern inline channel *channel_dup(channel *); \
    extern inline channel_waiter_ *channel_waitq_push_( \
        channel_waiter_ *_Atomic *, \
        ch_sem_ *, \
        _Atomic uint32_t *, \
        uint32_t, \
        void *); \
    extern inline bool channel_waitq_remove_( \
        channel_waiter_ *_Atomic *, channel_waiter_ *); \
    extern inline channel_waiter_ *channel_waitq_shift_( \
        channel_waiter_ *_Atomic *); \
    extern inline void channel_close(channel *); \
    extern inline channel *channel_drop(channel *); \
    extern inline channel_rc channel_buf_trysend_res_cell_( \
        channel_buf_ *, char **, uint32_t *); \
    extern inline void channel_buf_waitq_shift_( \
        channel_waiter_ *_Atomic *, ch_mutex_ *); \
    extern inline channel_rc channel_buf_trysend_(channel_buf_ *, void *); \
    extern inline channel_rc channel_unbuf_trysend_try_( \
        channel_unbuf_ *, channel_waiter_unbuf_ **); \
    extern inline channel_rc channel_unbuf_trysend_( \
        channel_unbuf_ *, void *); \
    extern inline channel_rc channel_trysend(channel *, void *, size_t); \
    extern inline channel_rc channel_buf_tryrecv_res_cell_( \
        channel_buf_ *, char **, uint32_t *); \
    extern inline channel_rc channel_buf_tryrecv_(channel_buf_ *, void *); \
    extern inline channel_rc channel_unbuf_tryrecv_( \
        channel_unbuf_ *, void *); \
    extern inline channel_rc channel_tryrecv(channel *, void *, size_t); \
    extern inline channel_rc channel_buf_forcesend_res_cell_( \
        channel_buf_ *, char **, uint32_t *); \
    extern inline channel_rc channel_forcesend(channel *, void *, size_t); \
    extern inline channel_rc channel_buf_send_or_add_waiter_( \
        channel_buf_ *, void *, ch_sem_ *, channel_waiter_ **); \
    extern inline channel_rc channel_buf_send_(channel_buf_ *, void *); \
    extern inline channel_rc channel_unbuf_send_or_add_waiter_( \
        channel_unbuf_ *, void *, ch_sem_ *, channel_waiter_ **); \
    extern inline channel_rc channel_unbuf_send_(channel_unbuf_ *, void *); \
    extern inline channel_rc channel_send(channel *, void *, size_t); \
    extern inline channel_rc channel_buf_recv_or_add_waiter_( \
        channel_buf_ *, void *, ch_sem_ *, channel_waiter_ **); \
    extern inline channel_rc channel_buf_recv_(channel_buf_ *, void *); \
    extern inline channel_rc channel_unbuf_recv_or_add_waiter_( \
        channel_unbuf_ *, void *, ch_sem_ *, channel_waiter_ **); \
    extern inline channel_rc channel_unbuf_recv_(channel_unbuf_ *, void *); \
    extern inline channel_rc channel_recv(channel *, void *, size_t); \
    extern inline ch_timespec_ channel_add_timeout_(uint64_t); \
    extern inline channel_rc channel_buf_timedsend_( \
        channel_buf_ *, void *, ch_timespec_ *); \
    extern inline channel_rc channel_unbuf_timedsend_( \
        channel_unbuf_ *, void *, ch_timespec_ *); \
    extern inline channel_rc channel_timedsend( \
        channel *, void *, uint64_t, size_t); \
    extern inline channel_rc channel_buf_timedrecv_( \
        channel_buf_ *, void *, ch_timespec_ *); \
    extern inline channel_rc channel_unbuf_timedrecv_( \
        channel_unbuf_ *, void *, ch_timespec_ *); \
    extern inline channel_rc channel_timedrecv( \
        channel *, void *, uint64_t, size_t); \
    extern inline channel_set *channel_set_make(uint32_t); \
    extern inline channel_set *channel_set_drop(channel_set *); \
    extern inline uint32_t channel_set_add( \
        channel_set *, channel *, channel_op, void *, size_t); \
    extern inline void channel_set_rereg( \
        channel_set *, uint32_t, channel_op, void *, size_t); \
    extern inline uint32_t channel_select_test_all_(channel_set *, uint32_t); \
    extern inline bool channel_select_ready_(channel *, channel_op); \
    extern inline channel_select_rc_ channel_select_ready_or_wait_( \
        channel_set *, _Atomic uint32_t *, uint32_t); \
    extern inline void channel_select_remove_waiters_( \
        channel_set *, uint32_t); \
    extern inline uint32_t channel_select(channel_set *, uint64_t)

/* `CHANNEL_PAD_CACHE_LINES` can be defined to enable cache line padding,
 * improving performance in highly parallel workloads. */
#ifdef CHANNEL_PAD_CACHE_LINES
#define CH_CACHE_LINE_ 64
#define CH_PAD_(id, size) char pad##id[size];
#else
#define CH_PAD_(id, size)
#endif

/* ---------------------------- Implementation ---------------------------- */
#ifdef _POSIX_THREADS // Linux, OS X, and Cygwin (and BSDs--untested, however)
#define ch_mutex_ pthread_mutex_t
#define ch_mutex_init_(m) (m) = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER
#define ch_mutex_destroy_(m) pthread_mutex_destroy(m)
#define ch_mutex_lock_(m) pthread_mutex_lock(m)
#define ch_mutex_unlock_(m) pthread_mutex_unlock(m)
#if _POSIX_SEMAPHORES >= 200112l // Linux and Cygwin (and BSDs?)
#define ch_sem_ sem_t
#define ch_sem_init_(sem, pshared, val) sem_init(sem, pshared, val)
#define ch_sem_post_(sem) sem_post(sem)
#define ch_sem_wait_(sem) channel_sem_wait_(sem)
#define ch_sem_timedwait_(sem, ts) channel_sem_timedwait_(sem, ts)
#define ch_sem_destroy_(sem) sem_destroy(sem)
#define ch_timespec_ struct timespec
#define CHANNEL_SEM_WAIT_DECL_ extern inline int channel_sem_wait_(sem_t *);
#define CHANNEL_SEM_TIMEDWAIT_DECL_ \
    extern inline int channel_sem_timedwait_(sem_t *, const struct timespec *);

inline int
channel_sem_wait_(sem_t *sem) {
    int rc;
    while ((rc = sem_wait(sem)) != 0) {
        if (errno == EINTR) {
            errno = 0;
            continue;
        }
        return rc;
    }
    return 0;
}

inline int
channel_sem_timedwait_(sem_t *sem, const struct timespec *ts) {
    int rc;
    while ((rc = sem_timedwait(sem, ts)) != 0) {
        if (errno == EINTR) {
            errno = 0;
            continue;
        }
        return rc;
    }
    return 0;
}
#elif defined __APPLE__ // OS X (and FreeBSD? Swap with previous case?)
#define ch_sem_ dispatch_semaphore_t
#define ch_sem_init_(sem, pshared, val) *(sem) = dispatch_semaphore_create(val)
#define ch_sem_post_(sem) dispatch_semaphore_signal(*(sem))
#define ch_sem_wait_(sem) \
    dispatch_semaphore_wait(*(sem), DISPATCH_TIME_FOREVER)
#define ch_sem_timedwait_(sem, ts) dispatch_semaphore_wait(*(sem), *(ts))
#define ch_sem_destroy_(sem) dispatch_release(*(sem))
#define ch_timespec_ dispatch_time_t
#define CHANNEL_SEM_WAIT_DECL_
#define CHANNEL_SEM_TIMEDWAIT_DECL_
#endif
#endif

/* TODO Consider replacing the wait queues with ring buffers. Linked lists were
 * chosen because constant-time removal regardless of position is, in theory,
 * nice for timeouts but ring buffers are much more performant in general. */
typedef struct channel_waiter_hdr_ {
    union channel_waiter_ *prev, *next;
    ch_sem_ *sem;
    _Atomic uint32_t *sel_state;
    uint32_t sel_id;
    _Atomic bool ref;
} channel_waiter_hdr_;

/* Buffered waiters currently only use the fields in the shared header. */
typedef struct channel_waiter_hdr_ channel_waiter_buf_;

typedef struct channel_waiter_unbuf_ {
    struct channel_waiter_unbuf_ *prev, *next;
    ch_sem_ *sem;
    _Atomic uint32_t *sel_state;
    uint32_t sel_id;
    _Atomic bool ref;
    bool closed;
    void *elt;
} channel_waiter_unbuf_;

typedef union channel_waiter_ {
    channel_waiter_hdr_ hdr;
    channel_waiter_buf_ buf;
    channel_waiter_unbuf_ unbuf;
} channel_waiter_;

typedef struct channel_hdr_ {
    uint32_t cap;
    _Atomic uint32_t refc;
    size_t eltsize;
    channel_waiter_ *_Atomic sendq, *_Atomic recvq;
    ch_mutex_ lock;
} channel_hdr_;

/* If C had generics, the cell struct would be defined as follows:
 * typedef struct channel_cell_<T> {
 *     _Atomic uint32_t lap;
 *     T elt;
 * } channel_cell_<T>; */
#define ch_cellsize_(eltsize) (sizeof(uint32_t) + eltsize)
#define ch_cell_lap_(cell) ((_Atomic uint32_t *)cell)
#define ch_cell_elt_(cell) (cell + sizeof(uint32_t))

typedef union channel_aun64_ {
    _Atomic uint64_t u64;
    struct {
        _Atomic uint32_t index;
        _Atomic uint32_t lap;
    } u32;
} channel_aun64_;

typedef union channel_un64_ {
    uint64_t u64;
    struct {
        uint32_t index;
        uint32_t lap;
    } u32;
} channel_un64_;

typedef struct channel_buf_ {
    uint32_t cap;
    _Atomic uint32_t refc;
    size_t eltsize;
    channel_waiter_ *_Atomic sendq, *_Atomic recvq;
    ch_mutex_ lock;
    channel_aun64_ write;
        CH_PAD_(0, CH_CACHE_LINE_ - sizeof(channel_aun64_))
    channel_aun64_ read;
        CH_PAD_(1, CH_CACHE_LINE_ - sizeof(channel_aun64_))
    char buf[]; // channel_cell_<T> buf[];
} channel_buf_;

/* Unbuffered channels currently only use the fields in the shared header. */
typedef struct channel_hdr_ channel_unbuf_;

union channel {
    channel_hdr_ hdr;
    channel_buf_ buf;
    channel_unbuf_ unbuf;
};

typedef struct channel_case_ {
    channel *c;
    void *elt;
    channel_waiter_ *waiter;
    channel_op op;
} channel_case_;

struct channel_set {
    uint32_t len, cap;
    channel_case_ *arr;
    ch_sem_ sem;
};

typedef enum channel_select_rc_ {
    CH_SEL_RC_READY_,
    CH_SEL_RC_WAIT_,
    CH_SEL_RC_CLOSED_,
} channel_select_rc_;

#define CH_SEL_MAGIC_ UINT32_MAX
#define CH_SEL_NIL_ (UINT32_MAX - 1)

#define ch_load_rlx_(obj) atomic_load_explicit(obj, memory_order_relaxed)
#define ch_load_acq_(obj) atomic_load_explicit(obj, memory_order_acquire)
#define ch_load_seq_cst_(obj) atomic_load_explicit(obj, memory_order_seq_cst)
#define ch_store_rlx_(obj, des) \
    atomic_store_explicit(obj, des, memory_order_relaxed)
#define ch_store_rel_(obj, des) \
    atomic_store_explicit(obj, des, memory_order_release)
#define ch_store_seq_cst_(obj, des) \
    atomic_store_explicit(obj, des, memory_order_seq_cst)
#define ch_faa_acq_rel_(obj, arg) \
    atomic_fetch_add_explicit(obj, arg, memory_order_acq_rel)
#define ch_fas_acq_rel_(obj, arg) \
    atomic_fetch_sub_explicit(obj, arg, memory_order_acq_rel)
#define ch_cas_weak_(obj, exp, des) \
    atomic_compare_exchange_weak_explicit( \
        obj, exp, des, memory_order_seq_cst, memory_order_relaxed)
#define ch_cas_strong_(obj, exp, des) \
    atomic_compare_exchange_strong_explicit( \
        obj, exp, des, memory_order_seq_cst, memory_order_relaxed)

/* `ch_assert_` never becomes a noop, even when `NDEBUG` is set. */
#define ch_assert_(pred) \
    (__builtin_expect(!(pred), 0) ? \
        channel_assert_(__FILE__, __LINE__, #pred) : (void)0)

__attribute__((noreturn)) inline void
channel_assert_(const char *file, unsigned line, const char *pred) {
    fprintf(stderr, "Failed assertion: %s, %u, %s\n", file, line, pred);
    abort();
}

/* Allocates and initializes a new channel. If the capacity is 0 then the
 * channel is unbuffered. Otherwise the channel is buffered. */
inline channel *
channel_make(size_t eltsize, uint32_t cap) {
    channel *c;
    if (cap > 0) {
        ch_assert_((c = calloc(
            1, offsetof(channel_buf_, buf) + (cap * ch_cellsize_(eltsize)))));
        ch_store_rlx_(&c->buf.read.u32.lap, 1);
    } else {
        ch_assert_((c = calloc(1, sizeof(c->unbuf))));
    }
    c->hdr.cap = cap;
    ch_store_rlx_(&c->hdr.refc, 1);
    c->hdr.eltsize = eltsize;
    ch_mutex_init_(c->hdr.lock);
    return c;
}

/* Increases the reference count of the channel and returns it. Intended to be
 * used with multiple producers that independently close the channel. */
inline channel *
channel_dup(channel *c) {
    ch_assert_(ch_faa_acq_rel_(&c->hdr.refc, 1) != 0);
    return c;
}

inline channel_waiter_ *
channel_waitq_push_(
    channel_waiter_ *_Atomic *waitq,
    ch_sem_ *sem,
    _Atomic uint32_t *state,
    uint32_t sel_id,
    void *elt
) {
    channel_waiter_ *w;
    if (!elt) {
        ch_assert_((w = calloc(1, sizeof(w->buf))));
    } else {
        ch_assert_((w = calloc(1, sizeof(w->unbuf))));
        w->unbuf.elt = elt;
    }
    w->hdr.sem = sem;
    w->hdr.sel_state = state;
    w->unbuf.sel_id = sel_id;

    channel_waiter_ *wq = ch_load_acq_(waitq);
    if (!wq) {
        w->hdr.prev = w;
        ch_store_seq_cst_(waitq, w);
    } else {
        w->hdr.prev = wq->hdr.prev;
        w->hdr.prev->hdr.next = w;
        wq->hdr.prev = w;
    }
    return w;
}

inline bool
channel_waitq_remove_(channel_waiter_ *_Atomic *waitq, channel_waiter_ *w) {
    if (!w->hdr.prev) { // Already shifted off the front
        return false;
    }
    if (w->hdr.prev == w) { // `w` is the only element
        ch_store_seq_cst_(waitq, NULL);
        return true;
    }

    w->hdr.prev->hdr.next = w->hdr.next;
    if (w->hdr.next) {
        w->hdr.next->hdr.prev = w->hdr.prev;
    } else {
        ch_load_acq_(waitq)->hdr.prev = w->hdr.prev;
    }
    return true;
}

inline channel_waiter_ *
channel_waitq_shift_(channel_waiter_ *_Atomic *waitq) {
    channel_waiter_ *w = ch_load_acq_(waitq);
    if (w) {
        if (w->hdr.next) {
            w->hdr.next->hdr.prev = w->hdr.prev;
        }
        w->hdr.prev = NULL; // For channel_waitq_remove
        ch_store_rlx_(&w->hdr.ref, true);
        ch_store_seq_cst_(waitq, w->hdr.next);
    }
    return w;
}

/* Closes the channel if the caller has the last reference to the channel.
 * Decreases the reference count otherwise. Attempting to send on a closed
 * channel immediately returns an error code. Receiving on a closed channel
 * succeeds until the channel is emptied. */
inline void
channel_close(channel *c) {
    uint32_t refc = ch_fas_acq_rel_(&c->hdr.refc, 1);
    if (refc != 1) {
        ch_assert_(refc != 0);
        return;
    }

    ch_mutex_lock_(&c->hdr.lock);
    if (c->hdr.cap > 0) {
        channel_waiter_buf_ *w;
        while ((w = &channel_waitq_shift_(&c->buf.sendq)->buf)) {
            ch_sem_post_(w->sem);
        }
        while ((w = &channel_waitq_shift_(&c->buf.recvq)->buf)) {
            ch_sem_post_(w->sem);
        }
    } else {
        channel_waiter_unbuf_ *w;
        while ((w = &channel_waitq_shift_(&c->unbuf.sendq)->unbuf)) {
            w->closed = true;
            ch_sem_post_(w->sem);
        }
        while ((w = &channel_waitq_shift_(&c->unbuf.recvq)->unbuf)) {
            w->closed = true;
            ch_sem_post_(w->sem);
        }
    }
    ch_mutex_unlock_(&c->hdr.lock);
}

/* Deallocates all resources associated with the channel and returns `NULL`. */
inline channel *
channel_drop(channel *c) {
    ch_assert_(ch_mutex_destroy_(&c->hdr.lock) == 0);
    free(c);
    return NULL;
}

inline channel_rc
channel_buf_trysend_res_cell_(channel_buf_ *c, char **cell, uint32_t *lap) {
    for (int i = 0; ; ) {
        if (ch_load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }

        channel_un64_ write = {ch_load_acq_(&c->write.u64)};
        *cell = c->buf + (write.u32.index * ch_cellsize_(c->eltsize));
        *lap = ch_load_acq_(ch_cell_lap_(*cell));
        if (write.u32.lap != *lap) {
            if (i++ < 4) { // Improves benchmarks; harmful in normal usage?
                sched_yield();
                continue;
            }
            return CH_WBLOCK;
        }

        uint64_t write1 = write.u32.index + 1 < c->cap ?
                write.u64 + 1 : (uint64_t)(write.u32.lap + 2) << 32;
        if (ch_cas_weak_(&c->write.u64, &write.u64, write1)) {
            return CH_OK;
        }
    }
}

inline void
channel_buf_waitq_shift_(channel_waiter_ *_Atomic *waitq, ch_mutex_ *lock) {
    for ( ; ; ) {
        if (ch_load_acq_(waitq)) {
            ch_mutex_lock_(lock);
            channel_waiter_buf_ *w = &channel_waitq_shift_(waitq)->buf;
            ch_mutex_unlock_(lock);
            if (w) {
                if (w->sel_state) {
                    uint32_t magic = CH_SEL_MAGIC_;
                    if (!ch_cas_strong_(w->sel_state, &magic, w->sel_id)) {
                        ch_store_rel_(&w->ref, false);
                        continue;
                    }
                }
                ch_sem_post_(w->sem);
            }
        }
        return;
    }
}

inline channel_rc
channel_buf_trysend_(channel_buf_ *c, void *elt) {
    char *cell;
    uint32_t lap;
    channel_rc rc = channel_buf_trysend_res_cell_(c, &cell, &lap);
    if (rc != CH_OK) {
        return rc;
    }

    memcpy(ch_cell_elt_(cell), elt, c->eltsize);
    ch_store_seq_cst_(ch_cell_lap_(cell), lap + 1);
    channel_buf_waitq_shift_(&c->recvq, &c->lock);
    return CH_OK;
}

inline channel_rc
channel_unbuf_trysend_try_(channel_unbuf_ *c, channel_waiter_unbuf_ **w) {
    for ( ; ; ) {
        if (ch_load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }
        if (!ch_load_acq_(&c->recvq)) {
            return CH_WBLOCK;
        }

        ch_mutex_lock_(&c->lock);
        if (ch_load_acq_(&c->refc) == 0) {
            ch_mutex_unlock_(&c->lock);
            return CH_CLOSED;
        }
        *w = &channel_waitq_shift_(&c->recvq)->unbuf;
        ch_mutex_unlock_(&c->lock);
        if (!*w) {
            return CH_WBLOCK;
        }
        if ((*w)->sel_state) {
            uint32_t magic = CH_SEL_MAGIC_;
            if (!ch_cas_strong_((*w)->sel_state, &magic, (*w)->sel_id)) {
                ch_store_rel_(&(*w)->ref, false);
                continue;
            }
        }
        return CH_OK;
    }
}

inline channel_rc
channel_unbuf_trysend_(channel_unbuf_ *c, void *elt) {
    channel_waiter_unbuf_ *w;
    channel_rc rc = channel_unbuf_trysend_try_(c, &w);
    if (rc != CH_OK) {
        return rc;
    }

    memcpy(w->elt, elt, c->eltsize);
    ch_sem_post_(w->sem);
    return CH_OK;
}

/* Nonblocking sends fail on buffered channels if the channel is full and fail
 * on unbuffered channels if there is no waiting receiver. Returns `CH_OK` on
 * success, `CH_WBLOCK` on failure, or `CH_CLOSED` if the channel is closed. */
inline channel_rc
channel_trysend(channel *c, void *elt, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize);
    if (c->hdr.cap > 0) {
        return channel_buf_trysend_(&c->buf, elt);
    }
    return channel_unbuf_trysend_(&c->unbuf, elt);
}

inline channel_rc
channel_buf_tryrecv_res_cell_(channel_buf_ *c, char **cell, uint32_t *lap) {
    for (int i = 0; ; ) {
        channel_un64_ read = {ch_load_acq_(&c->read.u64)};
        *cell = c->buf + (read.u32.index * ch_cellsize_(c->eltsize));
        *lap = ch_load_acq_(ch_cell_lap_(*cell));
        if (read.u32.lap != *lap) {
            if (ch_load_acq_(&c->refc) == 0) {
                return CH_CLOSED;
            }
            if (i++ < 4) { // Improves benchmarks; harmful in normal usage?
                sched_yield();
                continue;
            }
            return CH_WBLOCK;
        }

        uint64_t read1 = read.u32.index + 1 < c->cap ?
                read.u64 + 1 : (uint64_t)(read.u32.lap + 2) << 32;
        if (ch_cas_weak_(&c->read.u64, &read.u64, read1)) {
            return CH_OK;
        }
    }
}

inline channel_rc
channel_buf_tryrecv_(channel_buf_ *c, void *elt) {
    char *cell;
    uint32_t lap;
    channel_rc rc = channel_buf_tryrecv_res_cell_(c, &cell, &lap);
    if (rc != CH_OK) {
        return rc;
    }

    memcpy(elt, ch_cell_elt_(cell), c->eltsize);
    ch_store_seq_cst_(ch_cell_lap_(cell), lap + 1);
    channel_buf_waitq_shift_(&c->sendq, &c->lock);
    return CH_OK;
}

inline channel_rc
channel_unbuf_tryrecv_(channel_unbuf_ *c, void *elt) {
    for ( ; ; ) {
        if (ch_load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }
        if (!ch_load_acq_(&c->sendq)) {
            return CH_WBLOCK;
        }

        ch_mutex_lock_(&c->lock);
        if (ch_load_acq_(&c->refc) == 0) {
            ch_mutex_unlock_(&c->lock);
            return CH_CLOSED;
        }
        channel_waiter_unbuf_ *w = &channel_waitq_shift_(&c->sendq)->unbuf;
        ch_mutex_unlock_(&c->lock);
        if (!w) {
            return CH_WBLOCK;
        }
        if (w->sel_state) {
            uint32_t magic = CH_SEL_MAGIC_;
            if (!ch_cas_strong_(w->sel_state, &magic, w->sel_id)) {
                ch_store_rel_(&w->ref, false);
                continue;
            }
        }
        memcpy(elt, w->elt, c->eltsize);
        ch_sem_post_(w->sem);
        return CH_OK;
    }
}

/* Nonblocking receives fail on buffered channels if the channel is empty and
 * fail on unbuffered channels if there is no waiting sender. Returns `CH_OK`
 * on success, `CH_WBLOCK` on failure, or `CH_CLOSED` if the channel is
 * closed. */
inline channel_rc
channel_tryrecv(channel *c, void *elt, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize);
    if (c->hdr.cap > 0) {
        return channel_buf_tryrecv_(&c->buf, elt);
    }
    return channel_unbuf_tryrecv_(&c->unbuf, elt);
}

inline channel_rc
channel_buf_forcesend_res_cell_(channel_buf_ *c, char **cell, uint32_t *lap) {
    for (bool full = false; ; ) {
        if (ch_load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }

        channel_un64_ write = {ch_load_acq_(&c->write.u64)};
        *cell = c->buf + (write.u32.index * ch_cellsize_(c->eltsize));
        *lap = ch_load_acq_(ch_cell_lap_(*cell));
        if (write.u32.lap != *lap) {
            full = true;
            channel_un64_ read = {ch_load_acq_(&c->read.u64)};
            char *cell1 = c->buf + (read.u32.index * ch_cellsize_(c->eltsize));
            uint32_t lap1 = ch_load_acq_(ch_cell_lap_(cell1));
            if (read.u32.lap != lap1) {
                continue;
            }

            uint64_t read1 = read.u32.index + 1 < c->cap ?
                    read.u64 + 1 : (uint64_t)(read.u32.lap + 2) << 32;
            if (ch_cas_weak_(&c->read.u64, &read.u64, read1)) {
                ch_store_seq_cst_(ch_cell_lap_(cell1), lap1 + 1);
            }
            continue;
        }

        uint64_t write1 = write.u32.index + 1 < c->cap ?
                write.u64 + 1 : (uint64_t)(write.u32.lap + 2) << 32;
        if (ch_cas_weak_(&c->write.u64, &write.u64, write1)) {
            return full ? CH_WBLOCK : CH_OK;
        }
    }
}

/* Forced sends on buffered channels do not block and overwrite the oldest
 * message if the buffer is full. Forced sends are not possible with unbuffered
 * channels. Returns `CH_OK` on success or `CH_CLOSED` if the channel is
 * closed. */
inline channel_rc
channel_forcesend(channel *c, void *elt, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize && c->hdr.cap > 0);
    char *cell;
    uint32_t lap;
    channel_rc rc = channel_buf_forcesend_res_cell_(&c->buf, &cell, &lap);
    if (rc != CH_CLOSED) {
        memcpy(ch_cell_elt_(cell), elt, c->buf.eltsize);
        ch_store_seq_cst_(ch_cell_lap_(cell), lap + 1);
        channel_buf_waitq_shift_(&c->buf.recvq, &c->buf.lock);
    }
    return rc;
}

inline channel_rc
channel_buf_send_or_add_waiter_(
    channel_buf_ *c, void *elt, ch_sem_ *sem, channel_waiter_ **w
) {
    for ( ; ; ) {
        channel_rc rc = channel_buf_trysend_(c, elt);
        if (rc != CH_WBLOCK) {
            return rc;
        }

        ch_mutex_lock_(&c->lock);
        if (ch_load_acq_(&c->refc) == 0) {
            ch_mutex_unlock_(&c->lock);
            return CH_CLOSED;
        }
        *w = channel_waitq_push_(&c->sendq, sem, NULL, CH_SEL_NIL_, NULL);
        channel_un64_ write = {ch_load_acq_(&c->write.u64)};
        char *cell = c->buf + (write.u32.index * ch_cellsize_(c->eltsize));
        if (write.u32.lap == ch_load_acq_(ch_cell_lap_(cell))) {
            channel_waitq_remove_(&c->sendq, *w);
            ch_mutex_unlock_(&c->lock);
            free(*w);
            continue;
        }
        ch_sem_init_(sem, 0, 0);
        ch_mutex_unlock_(&c->lock);
        return CH_WBLOCK;
    }
}

inline channel_rc
channel_buf_send_(channel_buf_ *c, void *elt) {
    for ( ; ; ) {
        ch_sem_ sem;
        channel_waiter_ *w;
        channel_rc rc = channel_buf_send_or_add_waiter_(c, elt, &sem, &w);
        if (rc != CH_WBLOCK) {
            return rc;
        }

        ch_sem_wait_(&sem);
        ch_sem_destroy_(&sem);
        free(w);
    }
}

inline channel_rc
channel_unbuf_send_or_add_waiter_(
    channel_unbuf_ *c, void *elt, ch_sem_ *sem, channel_waiter_ **w
) {
    for ( ; ; ) {
        if (ch_load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }

        ch_mutex_lock_(&c->lock);
        if (ch_load_acq_(&c->refc) == 0) {
            ch_mutex_unlock_(&c->lock);
            return CH_CLOSED;
        }
        channel_waiter_unbuf_ *w1 = &channel_waitq_shift_(&c->recvq)->unbuf;
        if (w1) {
            ch_mutex_unlock_(&c->lock);
            if (w1->sel_state) {
                uint32_t magic = CH_SEL_MAGIC_;
                if (!ch_cas_strong_(w1->sel_state, &magic, w1->sel_id)) {
                    ch_store_rel_(&w1->ref, false);
                    continue;
                }
            }
            memcpy(w1->elt, elt, c->eltsize);
            ch_sem_post_(w1->sem);
            return CH_OK;
        }
        ch_sem_init_(sem, 0, 0);
        *w = channel_waitq_push_(&c->sendq, sem, NULL, CH_SEL_NIL_, elt);
        ch_mutex_unlock_(&c->lock);
        return CH_WBLOCK;
    }
}

inline channel_rc
channel_unbuf_send_(channel_unbuf_ *c, void *elt) {
    ch_sem_ sem;
    channel_waiter_ *w;
    channel_rc rc = channel_unbuf_send_or_add_waiter_(c, elt, &sem, &w);
    if (rc != CH_WBLOCK) {
        return rc;
    }

    ch_sem_wait_(&sem);
    rc = w->unbuf.closed ? CH_CLOSED : CH_OK;
    ch_sem_destroy_(&sem);
    free(w);
    return rc;
}

/* Blocking sends block on buffered channels if the buffer is full and block on
 * unbuffered channels if there is no waiting receiver. Returns `CH_OK` on
 * success or `CH_CLOSED` if the channel is closed. */
inline channel_rc
channel_send(channel *c, void *elt, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize);
    if (c->hdr.cap > 0) {
        return channel_buf_send_(&c->buf, elt);
    }
    return channel_unbuf_send_(&c->unbuf, elt);
}

inline channel_rc
channel_buf_recv_or_add_waiter_(
    channel_buf_ *c, void *elt, ch_sem_ *sem, channel_waiter_ **w
) {
    for ( ; ; ) {
        channel_rc rc = channel_buf_tryrecv_(c, elt);
        if (rc != CH_WBLOCK) {
            return rc;
        }

        ch_mutex_lock_(&c->lock);
        *w = channel_waitq_push_(&c->recvq, sem, NULL, CH_SEL_NIL_, NULL);
        channel_un64_ read = {ch_load_acq_(&c->read.u64)};
        char *cell = c->buf + (read.u32.index * ch_cellsize_(c->eltsize));
        if (read.u32.lap == ch_load_acq_(ch_cell_lap_(cell))) {
            channel_waitq_remove_(&c->recvq, *w);
            ch_mutex_unlock_(&c->lock);
            free(*w);
            continue;
        }
        if (ch_load_acq_(&c->refc) == 0) {
            channel_waitq_remove_(&c->recvq, *w);
            ch_mutex_unlock_(&c->lock);
            free(*w);
            return CH_CLOSED;
        }
        ch_sem_init_(sem, 0, 0);
        ch_mutex_unlock_(&c->lock);
        return CH_WBLOCK;
    }
}

inline channel_rc
channel_buf_recv_(channel_buf_ *c, void *elt) {
    for ( ; ; ) {
        ch_sem_ sem;
        channel_waiter_ *w;
        channel_rc rc = channel_buf_recv_or_add_waiter_(c, elt, &sem, &w);
        if (rc != CH_WBLOCK) {
            return rc;
        }

        ch_sem_wait_(&sem);
        ch_sem_destroy_(&sem);
        free(w);
    }
}

inline channel_rc
channel_unbuf_recv_or_add_waiter_(
    channel_unbuf_ *c, void *elt, ch_sem_ *sem, channel_waiter_ **w
) {
    for ( ; ; ) {
        if (ch_load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }

        ch_mutex_lock_(&c->lock);
        if (ch_load_acq_(&c->refc) == 0) {
            ch_mutex_unlock_(&c->lock);
            return CH_CLOSED;
        }
        channel_waiter_unbuf_ *w1 = &channel_waitq_shift_(&c->sendq)->unbuf;
        if (w1) {
            ch_mutex_unlock_(&c->lock);
            if (w1->sel_state) {
                uint32_t magic = CH_SEL_MAGIC_;
                if (!ch_cas_strong_(w1->sel_state, &magic, w1->sel_id)) {
                    ch_store_rel_(&w1->ref, false);
                    continue;
                }
            }
            memcpy(elt, w1->elt, c->eltsize);
            ch_sem_post_(w1->sem);
            return CH_OK;
        }
        ch_sem_init_(sem, 0, 0);
        *w = channel_waitq_push_(&c->recvq, sem, NULL, CH_SEL_NIL_, elt);
        ch_mutex_unlock_(&c->lock);
        return CH_WBLOCK;
    }
}

inline channel_rc
channel_unbuf_recv_(channel_unbuf_ *c, void *elt) {
    ch_sem_ sem;
    channel_waiter_ *w;
    channel_rc rc = channel_unbuf_recv_or_add_waiter_(c, elt, &sem, &w);
    if (rc != CH_WBLOCK) {
        return rc;
    }

    ch_sem_wait_(&sem);
    rc = w->unbuf.closed ? CH_CLOSED : CH_OK;
    ch_sem_destroy_(&sem);
    free(w);
    return rc;
}

/* Blocking receives block on buffered channels if the buffer is empty and
 * block on unbuffered channels if there is no waiting sender. Returns `CH_OK`
 * on success or `CH_CLOSED` if the channel is closed. */
inline channel_rc
channel_recv(channel *c, void *elt, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize);
    if (c->hdr.cap > 0) {
        return channel_buf_recv_(&c->buf, elt);
    }
    return channel_unbuf_recv_(&c->unbuf, elt);
}

/* Unfortunately we have to use `CLOCK_REALTIME` as there is no way to use
 * `CLOCK_MONOTIME` with `ch_sem_timedwait_`. */
inline ch_timespec_
channel_add_timeout_(uint64_t timeout) {
#ifdef __APPLE__
    dispatch_time_t ts = dispatch_time(DISPATCH_TIME_NOW, timeout * 1000000);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += (timeout % 1000) * 1000000;
    ts.tv_sec += ts.tv_nsec / 1000000000 +
            ((timeout - (timeout % 1000)) / 1000);
    ts.tv_nsec %= 1000000000;
#endif
    return ts;
}

inline channel_rc
channel_buf_timedsend_(channel_buf_ *c, void *elt, ch_timespec_ *timeout) {
    for ( ; ; ) {
        ch_sem_ sem;
        channel_waiter_ *w;
        channel_rc rc = channel_buf_send_or_add_waiter_(c, elt, &sem, &w);
        if (rc != CH_WBLOCK) {
            return rc;
        }

        if (ch_sem_timedwait_(&sem, timeout) != 0) {
            ch_mutex_lock_(&c->lock);
            bool onqueue = channel_waitq_remove_(&c->sendq, w);
            ch_mutex_unlock_(&c->lock);
            if (!onqueue) {
                ch_sem_wait_(w->buf.sem);
                channel_rc rc = channel_buf_trysend_(c, elt);
                if (rc != CH_WBLOCK) {
                    ch_sem_destroy_(&sem);
                    free(w);
                    return rc;
                }
            }
            ch_sem_destroy_(&sem);
            free(w);
            return CH_WBLOCK;
        }
        ch_sem_destroy_(&sem);
        free(w);
    }
}

inline channel_rc
channel_unbuf_timedsend_(channel_unbuf_ *c, void *elt, ch_timespec_ *timeout) {
    ch_sem_ sem;
    channel_waiter_ *w;
    channel_rc rc = channel_unbuf_send_or_add_waiter_(c, elt, &sem, &w);
    if (rc != CH_WBLOCK) {
        return rc;
    }
    if (ch_load_acq_(&c->refc) == 0) {
        return CH_CLOSED;
    }

    if (ch_sem_timedwait_(&sem, timeout) != 0) {
        ch_mutex_lock_(&c->lock);
        bool onqueue = channel_waitq_remove_(&c->sendq, w);
        ch_mutex_unlock_(&c->lock);
        if (onqueue) {
            rc = CH_WBLOCK;
            goto cleanup;
        }
        ch_sem_wait_(w->unbuf.sem);
    }
    rc = w->unbuf.closed ? CH_CLOSED : CH_OK;
cleanup:
    ch_sem_destroy_(&sem);
    free(w);
    return rc;
}

/* Timed sends fail on buffered channels if the channel is full for the
 * duration of the timeout and fail on unbuffered channels if there is no
 * waiting receiver for the duration of the timeout. Returns `CH_OK` on
 * success, `CH_WBLOCK` on failure, or `CH_CLOSED` if the channel is closed. */
inline channel_rc
channel_timedsend(channel *c, void *elt, uint64_t timeout, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize);
    ch_timespec_ t = channel_add_timeout_(timeout);
    if (c->hdr.cap > 0) {
        return channel_buf_timedsend_(&c->buf, elt, &t);
    }
    return channel_unbuf_timedsend_(&c->unbuf, elt, &t);
}

inline channel_rc
channel_buf_timedrecv_(channel_buf_ *c, void *elt, ch_timespec_ *timeout) {
    for ( ; ; ) {
        ch_sem_ sem;
        channel_waiter_ *w;
        channel_rc rc = channel_buf_recv_or_add_waiter_(c, elt, &sem, &w);
        if (rc != CH_WBLOCK) {
            return rc;
        }

        if (ch_sem_timedwait_(&sem, timeout) != 0) {
            ch_mutex_lock_(&c->lock);
            bool onqueue = channel_waitq_remove_(&c->recvq, w);
            ch_mutex_unlock_(&c->lock);
            if (!onqueue) {
                ch_sem_wait_(w->buf.sem);
                channel_rc rc = channel_buf_tryrecv_(c, elt);
                if (rc != CH_WBLOCK) {
                    ch_sem_destroy_(&sem);
                    free(w);
                    return rc;
                }
            }
            ch_sem_destroy_(&sem);
            free(w);
            return CH_WBLOCK;
        }
        ch_sem_destroy_(&sem);
        free(w);
    }
}

inline channel_rc
channel_unbuf_timedrecv_(channel_unbuf_ *c, void *elt, ch_timespec_ *timeout) {
    ch_sem_ sem;
    channel_waiter_ *w;
    channel_rc rc = channel_unbuf_recv_or_add_waiter_(c, elt, &sem, &w);
    if (rc != CH_WBLOCK) {
        return rc;
    }

    if (ch_sem_timedwait_(&sem, timeout) != 0) {
        ch_mutex_lock_(&c->lock);
        bool onqueue = channel_waitq_remove_(&c->recvq, w);
        ch_mutex_unlock_(&c->lock);
        if (onqueue) {
            rc = CH_WBLOCK;
            goto cleanup;
        }
        ch_sem_wait_(w->unbuf.sem);
    }
    rc = w->unbuf.closed ? CH_CLOSED : CH_OK;
cleanup:
    ch_sem_destroy_(&sem);
    free(w);
    return rc;
}

/* Timed receives fail on buffered channels if the channel is empty for the
 * duration of the timeout and fail on unbuffered channels if there is no
 * waiting sender for the duration of the timeout. Returns `CH_OK` on success,
 * `CH_WBLOCK` on failure, or `CH_CLOSED` if the channel is closed. */
inline channel_rc
channel_timedrecv(channel *c, void *elt, uint64_t timeout, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize);
    ch_timespec_ t = channel_add_timeout_(timeout);
    if (c->hdr.cap > 0) {
        return channel_buf_timedrecv_(&c->buf, elt, &t);
    }
    return channel_unbuf_timedrecv_(&c->unbuf, elt, &t);
}

/* Allocates and initializes a new channel set. `cap` is not a hard limit but a
 * `realloc` must be done every time it grows past the current capacity. */
inline channel_set *
channel_set_make(uint32_t cap) {
    channel_set *s = malloc(sizeof(*s));
    ch_assert_(s && cap != 0);
    ch_assert_((s->arr = malloc((s->cap = cap) * sizeof(s->arr[0]))));
    s->len = 0;
    ch_sem_init_(&s->sem, 0, 0);
    return s;
}

/* Deallocates all resources associated with the channel set and returns
 * `NULL`. */
inline channel_set *
channel_set_drop(channel_set *s) {
    ch_sem_destroy_(&s->sem);
    free(s->arr);
    free(s);
    return NULL;
}

/* Adds a channel to the channel set and registers the specified operation and
 * element. */
inline uint32_t
channel_set_add(
    channel_set *s,
    channel *c,
    channel_op op,
    void *elt,
    size_t eltsize
) {
    if (s->len == s->cap) {
        ch_assert_((
            s->arr = realloc(s->arr, (s->cap *= 2) * sizeof(s->arr[0]))));
    }
    ch_assert_(eltsize == c->hdr.eltsize &&
            s->len < CH_SEL_NIL_ &&
            s->len < RAND_MAX);
    s->arr[s->len] = (channel_case_){c, elt, NULL, op};
    return s->len++;
}

/* Change the registered op or element of the specified channel in the channel
 * set. */
inline void
channel_set_rereg(
    channel_set *s,
    uint32_t id,
    channel_op op,
    void *elt,
    size_t eltsize
) {
    ch_assert_(eltsize == s->arr[id].c->hdr.eltsize);
    s->arr[id].op = op;
    s->arr[id].elt = elt;
}

inline uint32_t
channel_select_test_all_(channel_set *s, uint32_t offset) {
    for (uint32_t i = 0; i < s->len; i++) {
        channel_case_ cc = s->arr[(i + offset) % s->len];
        if ((cc.op == CH_SEND &&
                channel_trysend(cc.c, cc.elt, cc.c->hdr.eltsize) == CH_OK) ||
            (cc.op == CH_RECV &&
                channel_tryrecv(cc.c, cc.elt, cc.c->hdr.eltsize) == CH_OK)) {
            return (i + offset) % s->len;
        }
    }
    return CH_SEL_NIL_;
}

inline bool
channel_select_ready_(channel *c, channel_op op) {
    if (c->hdr.cap > 0) {
        channel_un64_ u;
        if (op == CH_SEND) {
            u.u64 = ch_load_acq_(&c->buf.write.u64);
        } else {
            u.u64 = ch_load_acq_(&c->buf.read.u64);
        }
        char *cell = c->buf.buf + (u.u32.index * ch_cellsize_(c->buf.eltsize));
        return u.u32.lap == ch_load_acq_(ch_cell_lap_(cell));
    }

    if (op == CH_SEND) { // Could just do the operation here?
        return ch_load_acq_(&c->unbuf.recvq);
    }
    return ch_load_acq_(&c->unbuf.sendq);
}

inline channel_select_rc_
channel_select_ready_or_wait_(
    channel_set *s, _Atomic uint32_t *state, uint32_t offset
) {
    uint32_t closedc = 0;
    for (uint32_t i = 0; i < s->len; i++) {
        channel_case_ *cc = s->arr + ((i + offset) % s->len);
        if (cc->op == CH_NOOP || ch_load_acq_(&cc->c->hdr.refc) == 0) {
            closedc++;
            continue;
        }

        void *elt = cc->c->hdr.cap > 0 ? NULL : cc->elt;
        channel_waiter_ *_Atomic *waitq = cc->op == CH_SEND ?
                &cc->c->hdr.sendq : &cc->c->hdr.recvq;
        ch_mutex_lock_(&cc->c->hdr.lock);
        cc->waiter = channel_waitq_push_(
            waitq, &s->sem, state, (i + offset) % s->len, elt);
        if (channel_select_ready_(cc->c, cc->op)) {
            channel_waitq_remove_(waitq, cc->waiter);
            ch_mutex_unlock_(&cc->c->hdr.lock);
            free(cc->waiter);
            cc->waiter = NULL;
            return CH_SEL_RC_READY_;
        }
        ch_mutex_unlock_(&cc->c->hdr.lock);
    }

    if (closedc == s->len) {
        return CH_SEL_RC_CLOSED_;
    }
    return CH_SEL_RC_WAIT_;
}

inline void
channel_select_remove_waiters_(channel_set *s, uint32_t state) {
    for (uint32_t i = 0; i < s->len; i++) {
        channel_case_ *cc = s->arr + i;
        if (cc->op == CH_NOOP ||
                ch_load_acq_(&cc->c->hdr.refc) == 0 ||
                !cc->waiter) {
            continue;
        }
        channel_waiter_ *_Atomic *waitq = cc->op == CH_SEND ?
                &cc->c->hdr.sendq : &cc->c->hdr.recvq;
        ch_mutex_lock_(&cc->c->hdr.lock);
        bool onqueue = channel_waitq_remove_(waitq, cc->waiter);
        ch_mutex_unlock_(&cc->c->hdr.lock);
        if (!onqueue && i != state) {
            while (ch_load_acq_(&cc->waiter->hdr.ref)) {
                sched_yield();
            }
        }
        free(cc->waiter);
        cc->waiter = NULL;
    }
}

/* Randomly performs the registered operation on one channel in the channel set
 * that is ready to perform that operation. Blocks if no channel is ready.
 * Returns the id of the channel successfully completes its operation or
 * `CH_SEL_CLOSED` if all channels are closed or have been registered with
 * `CH_NOOP`. */
inline uint32_t
channel_select(channel_set *s, uint64_t timeout) {
    uint32_t offset = rand() % s->len;
    ch_timespec_ t;
    if (timeout > 0) {
        t = channel_add_timeout_(timeout);
    }
    for ( ; ; ) {
        uint32_t rc = channel_select_test_all_(s, offset);
        if (rc != CH_SEL_NIL_) {
            return rc;
        }

        _Atomic uint32_t state;
        uint32_t magic = CH_SEL_MAGIC_;
        bool timedout = false;
        ch_store_rlx_(&state, magic);
        switch (channel_select_ready_or_wait_(s, &state, offset)) {
        case CH_SEL_RC_CLOSED_:
            return CH_SEL_CLOSED;
        case CH_SEL_RC_READY_:
            ch_cas_strong_(&state, &magic, CH_SEL_NIL_);
            if (state < CH_SEL_NIL_) {
                ch_sem_wait_(&s->sem);
            }
            break;
        case CH_SEL_RC_WAIT_:
            if (timeout > 0) {
                if (ch_sem_timedwait_(&s->sem, &t) != 0) {
                    timedout = true;
                    ch_cas_strong_(&state, &magic, CH_SEL_NIL_);
                    if (state < CH_SEL_NIL_) {
                        ch_sem_wait_(&s->sem);
                    }
                    break;
                }
            } else {
                ch_sem_wait_(&s->sem);
            }
            ch_cas_strong_(&state, &magic, CH_SEL_NIL_);
        }

        channel_select_remove_waiters_(s, magic);
        if (state < CH_SEL_NIL_) {
            channel_case_ cc = s->arr[state];
            if (cc.c->hdr.cap == 0 ||
                    (cc.op == CH_SEND && channel_buf_trysend_(
                        &cc.c->buf, cc.elt) == CH_OK) ||
                    (cc.op == CH_RECV && channel_buf_tryrecv_(
                        &cc.c->buf, cc.elt) == CH_OK)) {
                return state;
            }
        }
        if (timedout) {
            return CH_SEL_WBLOCK;
        }
    }
}

/* `ch_poll` is an alternative to `ch_select`. It continuously loops over the
 * cases (`fn` is intended to be either `ch_trysend` or `ch_tryrecv`) rather
 * than blocking, which may be preferable in cases where one of the functions
 * is expected to succeed or if there is a default case but burns a lot of
 * cycles otherwise. */
#define ch_poll(casec) if (1) { \
    bool Xdone_ = false; \
    switch (rand() % casec) { \
    for ( ; ; Xdone_ = true)

#define ch_case(id, fn, ...) \
    case id: \
        if (fn == CH_OK) { \
            __VA_ARGS__; \
            break; \
        } else (void)0

#define ch_default(...) \
    if (Xdone_) { \
        __VA_ARGS__; \
        break; \
    } else (void)0

#define ch_poll_end } } else (void)0
#endif
