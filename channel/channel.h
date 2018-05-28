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
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if _POSIX_SEMAPHORES >= 200112l
#include <semaphore.h>
#include <time.h>
#elif defined __APPLE__
#include <dispatch/dispatch.h>
#else
#error "Do something about this later."
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
#define CH_OK 0x0
#define CH_CLOSED 0x1
#define CH_FULL 0x2
#define CH_EMPTY 0x4
#define CH_TIMEDOUT 0x8

#define CH_SEL_CLOSED UINT32_MAX // Should be merged somehow with CH_CLOSED?
#define CH_SEL_TIMEDOUT (UINT32_MAX - 1) // Same

/* Op codes */
typedef enum channel_op {
    CH_NOOP,
    CH_SEND,
    CH_RECV,
} channel_op;

/* Exported "functions" */
#define ch_make(T, cap) channel_make(sizeof(T), cap)
#define ch_dup(c) channel_dup(c)
#define ch_close(c) channel_close(c)
#define ch_drop(c) channel_drop(c)

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
    extern inline void channel_assert_( \
        const char *, unsigned, const char *) __attribute__((noreturn)); \
    extern inline channel *channel_make(size_t, uint32_t); \
    extern inline channel *channel_dup(channel *); \
    extern inline channel_waiter_ *channel_waitq_push_( \
        channel_waiter_ *_Atomic *, \
        sem_t *, \
        _Atomic uint32_t *, \
        uint32_t, \
        void *); \
    extern inline bool channel_waitq_remove( \
        channel_waiter_ *_Atomic *, channel_waiter_ *); \
    extern inline channel_waiter_ *channel_waitq_shift_( \
        channel_waiter_ *_Atomic *); \
    extern inline void channel_close(channel *); \
    extern inline channel *channel_drop(channel *); \
    extern inline int channel_buf_res_cell_( \
        channel_buf_ *, char **, uint32_t *); \
    extern inline void channel_buf_waitq_shift_( \
        channel_waiter_ *_Atomic *, pthread_mutex_t *); \
    extern inline int channel_buf_trysend_(channel_buf_ *, void *); \
    extern inline int channel_unbuf_trysend_try_( \
        channel_unbuf_ *, channel_waiter_unbuf_ **); \
    extern inline int channel_unbuf_trysend_(channel_unbuf_ *, void *); \
    extern inline int channel_trysend(channel *, void *, size_t); \
    extern inline int channel_buf_tryrecv_(channel_buf_ *, void *); \
    extern inline int channel_unbuf_tryrecv_(channel_unbuf_ *, void *); \
    extern inline int channel_tryrecv(channel *, void *, size_t); \
    extern inline int channel_buf_forceres_cell_( \
        channel_buf_ *, char **, uint32_t *); \
    extern inline int channel_forcesend(channel *, void *, size_t); \
    extern inline int channel_buf_send_or_add_waiter_( \
        channel_buf_ *, void *, sem_t *, channel_waiter_ **); \
    extern inline int channel_buf_send_(channel_buf_ *, void *); \
    extern inline int channel_unbuf_send_or_add_waiter_( \
        channel_unbuf_ *, void *, sem_t *, channel_waiter_ **); \
    extern inline int channel_unbuf_send_(channel_unbuf_ *, void *); \
    extern inline int channel_send(channel *, void *, size_t); \
    extern inline int channel_buf_recv_or_add_waiter_( \
        channel_buf_ *, void *, sem_t *, channel_waiter_ **); \
    extern inline int channel_buf_recv_(channel_buf_ *, void *); \
    extern inline int channel_unbuf_recv_or_add_waiter_( \
        channel_unbuf_ *, void *, sem_t *, channel_waiter_ **); \
    extern inline int channel_unbuf_recv_(channel_unbuf_ *, void *); \
    extern inline int channel_recv(channel *, void *, size_t); \
    extern inline ch_timespec_ channel_add_timeout_(uint64_t); \
    extern inline int channel_buf_timedsend_( \
        channel_buf_ *, void *, ch_timespec_ *); \
    extern inline int channel_unbuf_timedsend_( \
        channel_unbuf_ *, void *, ch_timespec_ *); \
    extern inline int channel_timedsend(channel *, void *, uint64_t, size_t); \
    extern inline int channel_buf_timedrecv_( \
        channel_buf_ *, void *, ch_timespec_ *); \
    extern inline int channel_unbuf_timedrecv_( \
        channel_unbuf_ *, void *, ch_timespec_ *); \
    extern inline int channel_timedrecv(channel *, void *, uint64_t, size_t); \
    extern inline channel_set *channel_set_make(uint32_t); \
    extern inline channel_set *channel_set_drop(channel_set *); \
    extern inline uint32_t channel_set_add( \
        channel_set *, channel *, channel_op, void *, size_t); \
    extern inline void channel_set_rereg( \
        channel_set *, uint32_t, channel_op, void *, size_t); \
    extern inline bool channel_select_test_all_( \
        channel_set *, uint32_t *, uint32_t); \
    extern inline bool channel_select_ready_(channel *, channel_op); \
    extern inline channel_select_rc_ channel_select_ready_or_wait_( \
        channel_set *, _Atomic uint32_t *, uint32_t); \
    extern inline void channel_select_remove_waiters_(channel_set *); \
    extern inline uint32_t channel_select(channel_set *, uint64_t)

/* `CHANNEL_PAD_CACHE_LINES` is left undefined by default as cache line padding
 * doesn't seem to make a difference in my tests. Feel free to define it if
 * your testing shows that it helps. */
#ifdef CHANNEL_PAD_CACHE_LINES
#define CH_CACHE_LINE_ 128
#define CH_PAD_(id, size) char pad##id[size];
#else
#define CH_PAD_(id, size)
#endif

/* ---------------------------- Implementation ---------------------------- */
#define CH_CONTINUE_ 0x10

#define CH_SEL_MAGIC_ UINT32_MAX
#define CH_SEL_NIL_ (UINT32_MAX - 1)

typedef enum channel_select_rc_ {
    CH_SEL_RC_READY_,
    CH_SEL_RC_WAIT_,
    CH_SEL_RC_CLOSED_,
} channel_select_rc_;

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

/* TODO Handle POSIX semaphore error codes. */
#if _POSIX_SEMAPHORES < 200112L && defined __APPLE__
#define sem_t dispatch_semaphore_t
#define sem_init(sem, pshared, val) *(sem) = dispatch_semaphore_create(val)
#define sem_post(sem) dispatch_semaphore_signal(*(sem))
#define sem_wait(sem) dispatch_semaphore_wait(*(sem), DISPATCH_TIME_FOREVER)
#define sem_timedwait(sem, time) dispatch_semaphore_wait(*(sem), *(time))
#define sem_destroy(sem) dispatch_release(*(sem))
#define ch_timespec_ dispatch_time_t
#elif _POSIX_TIMEOUTS >= 200112L
#define ch_timespec_ struct timespec
#else
#error "Do something about this later."
#endif

/* TODO Consider replacing the wait queues with ring buffers. Linked lists were
 * chosen because constant-time removal regardless of position is, in theory,
 * nice for timeouts but ring buffers are much more performant in general. */
typedef struct channel_waiter_hdr_ {
    union channel_waiter_ *prev, *next;
    sem_t *sem;
    _Atomic uint32_t *sel_state;
    uint32_t sel_id;
} channel_waiter_hdr_;

/* Buffered waiters currently only use the fields in the shared header. */
typedef struct channel_waiter_hdr_ channel_waiter_buf_;

typedef struct channel_waiter_unbuf_ {
    struct channel_waiter_unbuf_ *prev, *next;
    sem_t *sem;
    _Atomic uint32_t *sel_state;
    uint32_t sel_id;
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
        CH_PAD_(0, CH_CACHE_LINE_ - sizeof(uint32_t))
    _Atomic uint32_t refc;
        CH_PAD_(1, CH_CACHE_LINE_ - sizeof(_Atomic uint32_t))
    size_t eltsize;
        CH_PAD_(2, CH_CACHE_LINE_ - sizeof(size_t))
    channel_waiter_ *_Atomic sendq;
        CH_PAD_(3, CH_CACHE_LINE_ - sizeof(channel_waiter_ *_Atomic))
    channel_waiter_ *_Atomic recvq;
        CH_PAD_(4, CH_CACHE_LINE_ - sizeof(channel_waiter_ *_Atomic))
    pthread_mutex_t lock;
} channel_hdr_;

/* If C had generics, the cell struct would be defined as follows:
 * typedef struct channel_cell_<T> {
 *     _Atomic uint32_t lap;
 *     T elt;
 * } channel_cell_<T>; */
#define ch_cellsize_(eltsize) (sizeof(uint32_t) + eltsize)
#define ch_cell_lap_(cell) ((_Atomic uint32_t *)cell)
#define ch_cell_elt_(cell) (cell + sizeof(uint32_t))

typedef struct channel_buf_ {
    uint32_t cap;
        CH_PAD_(0, CH_CACHE_LINE_ - sizeof(uint32_t))
    _Atomic uint32_t refc;
        CH_PAD_(1, CH_CACHE_LINE_ - sizeof(_Atomic uint32_t))
    size_t eltsize;
        CH_PAD_(2, CH_CACHE_LINE_ - sizeof(size_t))
    channel_waiter_ *_Atomic sendq;
        CH_PAD_(3, CH_CACHE_LINE_ - sizeof(channel_waiter_ *_Atomic))
    channel_waiter_ *_Atomic recvq;
        CH_PAD_(4, CH_CACHE_LINE_ - sizeof(channel_waiter_ *_Atomic))
    pthread_mutex_t lock;
        CH_PAD_(5, CH_CACHE_LINE_ - (sizeof(pthread_mutex_t) % CH_CACHE_LINE_))
    channel_aun64_ write;
        CH_PAD_(6, CH_CACHE_LINE_ - sizeof(channel_aun64_))
    channel_aun64_ read;
        CH_PAD_(7, CH_CACHE_LINE_ - sizeof(channel_aun64_))
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
    sem_t *sem;
    void *elt;
    channel_waiter_ *waiter;
    channel_op op;
} channel_case_;

struct channel_set {
    uint32_t len, cap;
    channel_case_ *arr;
    sem_t sem;
};

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
    c->hdr.lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
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
    sem_t *sem,
    _Atomic uint32_t *state,
    uint32_t sel_id,
    void *elt
) {
    channel_waiter_ *w;
    if (!elt) {
        ch_assert_((w = malloc(sizeof(w->buf))));
    } else {
        ch_assert_((w = malloc(sizeof(w->unbuf))));
        w->unbuf.closed = false;
        w->unbuf.elt = elt;
    }
    w->hdr.next = NULL;
    w->hdr.sem = sem;
    w->hdr.sel_state = state;
    w->unbuf.sel_id = sel_id;

    channel_waiter_ *wq = ch_load_acq_(waitq);
    if (!wq) {
        ch_store_seq_cst_(waitq, w);
        w->hdr.prev = w;
    } else {
        w->hdr.prev = wq->hdr.prev;
        wq->hdr.prev = w;
        w->hdr.prev->hdr.next = w;
    }
    return w;
}

inline bool
channel_waitq_remove(channel_waiter_ *_Atomic *waitq, channel_waiter_ *w) {
    if (!w->hdr.prev) { // Already shifted off the front
        return false;
    }
    if (w->hdr.prev == w) { // `w` is the only element
        ch_store_seq_cst_(waitq, NULL);
        return true;
    }

    if (w->hdr.prev->hdr.next) {
        w->hdr.prev->hdr.next = w->hdr.next;
    }
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
        ch_store_seq_cst_(waitq, w->hdr.next);
        w->hdr.prev = NULL; // For channel_waitq_remove
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

    pthread_mutex_lock(&c->hdr.lock);
    if (c->hdr.cap > 0) {
        while (ch_load_acq_(&c->buf.sendq)) {
            channel_waiter_buf_ *w = &channel_waitq_shift_(&c->buf.sendq)->buf;
            sem_post(w->sem);
        }
        while (ch_load_acq_(&c->buf.recvq)) {
            channel_waiter_buf_ *w = &channel_waitq_shift_(&c->buf.recvq)->buf;
            sem_post(w->sem);
        }
    } else {
        while (ch_load_acq_(&c->unbuf.sendq)) {
            channel_waiter_unbuf_ *w = &channel_waitq_shift_(
                &c->unbuf.sendq)->unbuf;
            w->closed = true;
            sem_post(w->sem);
        }
        while (ch_load_acq_(&c->unbuf.recvq)) {
            channel_waiter_unbuf_ *w = &channel_waitq_shift_(
                &c->unbuf.recvq)->unbuf;
            w->closed = true;
            sem_post(w->sem);
        }
    }
    pthread_mutex_unlock(&c->hdr.lock);
}

/* Deallocates all resources associated with the channel and returns `NULL`. */
inline channel *
channel_drop(channel *c) {
    ch_assert_(pthread_mutex_destroy(&c->hdr.lock) == 0);
    free(c);
    return NULL;
}

inline int
channel_buf_res_cell_(channel_buf_ *c, char **cell, uint32_t *lap) {
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
            return CH_FULL;
        }

        uint64_t write1 = write.u32.index + 1 < c->cap ?
                write.u64 + 1 : (uint64_t)(write.u32.lap + 2) << 32;
        if (ch_cas_weak_(&c->write.u64, &write.u64, write1)) {
            return CH_OK;
        }
    }
}

inline void
channel_buf_waitq_shift_(
    channel_waiter_ *_Atomic *waitq, pthread_mutex_t *lock
) {
    for ( ; ; ) {
        if (ch_load_acq_(waitq)) {
            pthread_mutex_lock(lock);
            channel_waiter_buf_ *w = &channel_waitq_shift_(waitq)->buf;
            pthread_mutex_unlock(lock);
            if (w) {
                if (w->sel_state) {
                    uint32_t magic = CH_SEL_MAGIC_;
                    if (!ch_cas_strong_(w->sel_state, &magic, w->sel_id)) {
                        continue;
                    }
                }
                sem_post(w->sem);
            }
        }
        return;
    }
}

inline int
channel_buf_trysend_(channel_buf_ *c, void *elt) {
    char *cell;
    uint32_t lap;
    int rc = channel_buf_res_cell_(c, &cell, &lap);
    if (rc != CH_OK) {
        return rc;
    }

    memcpy(ch_cell_elt_(cell), elt, c->eltsize);
    ch_store_seq_cst_(ch_cell_lap_(cell), lap + 1);
    channel_buf_waitq_shift_(&c->recvq, &c->lock);
    return CH_OK;
}

inline int
channel_unbuf_trysend_try_(channel_unbuf_ *c, channel_waiter_unbuf_ **w) {
    for ( ; ; ) {
        if (ch_load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }
        if (!ch_load_acq_(&c->recvq)) {
            return CH_FULL;
        }

        pthread_mutex_lock(&c->lock);
        if (ch_load_acq_(&c->refc) == 0) {
            pthread_mutex_unlock(&c->lock);
            return CH_CLOSED;
        }
        *w = &channel_waitq_shift_(&c->recvq)->unbuf;
        pthread_mutex_unlock(&c->lock);
        if (!*w) {
            return CH_FULL;
        }
        if ((*w)->sel_state) {
            uint32_t magic = CH_SEL_MAGIC_;
            if (!ch_cas_strong_((*w)->sel_state, &magic, (*w)->sel_id)) {
                continue;
            }
        }
        return CH_OK;
    }
}

inline int
channel_unbuf_trysend_(channel_unbuf_ *c, void *elt) {
    channel_waiter_unbuf_ *w;
    int rc = channel_unbuf_trysend_try_(c, &w);
    if (rc != CH_OK) {
        return rc;
    }

    memcpy(w->elt, elt, c->eltsize);
    sem_post(w->sem);
    return CH_OK;
}

/* Nonblocking sends fail on buffered channels if the channel is full and fail
 * on unbuffered channels if there is no waiting receiver. Returns `CH_OK` on
 * success, `CH_FULL` on failure, or `CH_CLOSED` if the channel is closed. */
inline int
channel_trysend(channel *c, void *elt, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize);
    if (c->hdr.cap > 0) {
        return channel_buf_trysend_(&c->buf, elt);
    }
    return channel_unbuf_trysend_(&c->unbuf, elt);
}

/* TODO Break up like `ch_buf_trysend` in preparation for GNU11 version? */
inline int
channel_buf_tryrecv_(channel_buf_ *c, void *elt) {
    for (int i = 0; ; ) {
        channel_un64_ read = {ch_load_acq_(&c->read.u64)};
        char *cell = c->buf + (read.u32.index * ch_cellsize_(c->eltsize));
        uint32_t lap = ch_load_acq_(ch_cell_lap_(cell));
        if (read.u32.lap != lap) {
            if (ch_load_acq_(&c->refc) == 0) {
                return CH_CLOSED;
            }
            if (i++ < 4) { // Improves benchmarks; harmful in normal usage?
                sched_yield();
                continue;
            }
            return CH_EMPTY;
        }

        uint64_t read1 = read.u32.index + 1 < c->cap ?
                read.u64 + 1 : (uint64_t)(read.u32.lap + 2) << 32;
        if (ch_cas_weak_(&c->read.u64, &read.u64, read1)) {
            memcpy(elt, ch_cell_elt_(cell), c->eltsize);
            ch_store_seq_cst_(ch_cell_lap_(cell), lap + 1);
            channel_buf_waitq_shift_(&c->sendq, &c->lock);
            return CH_OK;
        }
    }
}

inline int
channel_unbuf_tryrecv_(channel_unbuf_ *c, void *elt) {
    for ( ; ; ) {
        if (ch_load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }
        if (!ch_load_acq_(&c->sendq)) {
            return CH_EMPTY;
        }

        pthread_mutex_lock(&c->lock);
        if (ch_load_acq_(&c->refc) == 0) {
            pthread_mutex_unlock(&c->lock);
            return CH_CLOSED;
        }
        channel_waiter_unbuf_ *w = &channel_waitq_shift_(&c->sendq)->unbuf;
        pthread_mutex_unlock(&c->lock);
        if (!w) {
            return CH_EMPTY;
        }
        if (w->sel_state) {
            uint32_t magic = CH_SEL_MAGIC_;
            if (!ch_cas_strong_(w->sel_state, &magic, w->sel_id)) {
                continue;
            }
        }
        memcpy(elt, w->elt, c->eltsize);
        sem_post(w->sem);
        return CH_OK;
    }
}

/* Nonblocking receives fail on buffered channels if the channel is empty and
 * fail on unbuffered channels if there is no waiting sender. Returns `CH_OK`
 * on success, `CH_EMPTY` on failure, or `CH_CLOSED` if the channel is closed.
 */
inline int
channel_tryrecv(channel *c, void *elt, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize);
    if (c->hdr.cap > 0) {
        return channel_buf_tryrecv_(&c->buf, elt);
    }
    return channel_unbuf_tryrecv_(&c->unbuf, elt);
}

inline int
channel_buf_forceres_cell_(channel_buf_ *c, char **cell, uint32_t *lap) {
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
            return full ? CH_FULL : CH_OK;
        }
    }
}

/* Forced sends on buffered channels do not block and overwrite the oldest
 * message if the buffer is full. Forced sends are not possible with unbuffered
 * channels. Returns `CH_OK` on success or `CH_CLOSED` if the channel is
 * closed.
 *
 * XXX Not well tested. */
inline int
channel_forcesend(channel *c, void *elt, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize && c->hdr.cap > 0);
    char *cell;
    uint32_t lap;
    int rc = channel_buf_forceres_cell_(&c->buf, &cell, &lap);
    if (rc != CH_CLOSED) {
        memcpy(ch_cell_elt_(cell), elt, c->buf.eltsize);
        ch_store_seq_cst_(ch_cell_lap_(cell), lap + 1);
        channel_buf_waitq_shift_(&c->buf.recvq, &c->buf.lock);
    }
    return rc;
}

inline int
channel_buf_send_or_add_waiter_(
    channel_buf_ *c, void *elt, sem_t *sem, channel_waiter_ **w
) {
    int rc = channel_buf_trysend_(c, elt);
    if (rc != CH_FULL) {
        return rc;
    }

    pthread_mutex_lock(&c->lock);
    if (ch_load_acq_(&c->refc) == 0) {
        pthread_mutex_unlock(&c->lock);
        return CH_CLOSED;
    }
    sem_init(sem, 0, 0);
    *w = channel_waitq_push_(&c->sendq, sem, NULL, CH_SEL_NIL_, NULL);
    channel_un64_ write = {ch_load_acq_(&c->write.u64)};
    char *cell = c->buf + (write.u32.index * ch_cellsize_(c->eltsize));
    if (write.u32.lap == ch_load_acq_(ch_cell_lap_(cell))) {
        channel_waitq_remove(&c->sendq, *w);
        pthread_mutex_unlock(&c->lock);
        sem_destroy(sem);
        free(*w);
        return CH_CONTINUE_;
    }
    pthread_mutex_unlock(&c->lock);
    return CH_FULL;
}

inline int
channel_buf_send_(channel_buf_ *c, void *elt) {
    for ( ; ; ) {
        sem_t sem;
        channel_waiter_ *w;
        int rc = channel_buf_send_or_add_waiter_(c, elt, &sem, &w);
        if (rc == CH_CONTINUE_) {
            continue;
        } else if (rc != CH_FULL) {
            return rc;
        }

        sem_wait(&sem);
        sem_destroy(&sem);
        free(w);
    }
}

inline int
channel_unbuf_send_or_add_waiter_(
    channel_unbuf_ *c, void *elt, sem_t *sem, channel_waiter_ **w
) {
    if (ch_load_acq_(&c->refc) == 0) {
        return CH_CLOSED;
    }

    pthread_mutex_lock(&c->lock);
    if (ch_load_acq_(&c->refc) == 0) {
        pthread_mutex_unlock(&c->lock);
        return CH_CLOSED;
    }
    channel_waiter_unbuf_ *w1 = &channel_waitq_shift_(&c->recvq)->unbuf;
    if (w1) {
        pthread_mutex_unlock(&c->lock);
        if (w1->sel_state) {
            uint32_t magic = CH_SEL_MAGIC_;
            if (!ch_cas_strong_(w1->sel_state, &magic, w1->sel_id)) {
                return CH_CONTINUE_;
            }
        }
        memcpy(w1->elt, elt, c->eltsize);
        sem_post(w1->sem);
        return CH_OK;
    }
    sem_init(sem, 0, 0);
    *w = channel_waitq_push_(&c->sendq, sem, NULL, CH_SEL_NIL_, elt);
    pthread_mutex_unlock(&c->lock);
    return CH_FULL;
}

inline int
channel_unbuf_send_(channel_unbuf_ *c, void *elt) {
    for ( ; ; ) {
        sem_t sem;
        channel_waiter_ *w;
        int rc = channel_unbuf_send_or_add_waiter_(c, elt, &sem, &w);
        if (rc == CH_CONTINUE_) {
            continue;
        } else if (rc != CH_FULL) {
            return rc;
        }

        sem_wait(&sem);
        rc = w->unbuf.closed ? CH_CLOSED : CH_OK;
        sem_destroy(&sem);
        free(w);
        return rc;
    }
}

/* Blocking sends block on buffered channels if the buffer is full and block on
 * unbuffered channels if there is no waiting receiver. Returns `CH_OK` on
 * success or `CH_CLOSED` if the channel is closed. */
inline int
channel_send(channel *c, void *elt, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize);
    if (c->hdr.cap > 0) {
        return channel_buf_send_(&c->buf, elt);
    }
    return channel_unbuf_send_(&c->unbuf, elt);
}

inline int
channel_buf_recv_or_add_waiter_(
    channel_buf_ *c, void *elt, sem_t *sem, channel_waiter_ **w
) {
    int rc = channel_buf_tryrecv_(c, elt);
    if (rc != CH_EMPTY) {
        return rc;
    }

    pthread_mutex_lock(&c->lock);
    sem_init(sem, 0, 0);
    *w = channel_waitq_push_(&c->recvq, sem, NULL, CH_SEL_NIL_, NULL);
    channel_un64_ read = {ch_load_acq_(&c->read.u64)};
    char *cell = c->buf + (read.u32.index * ch_cellsize_(c->eltsize));
    if (read.u32.lap == ch_load_acq_(ch_cell_lap_(cell))) {
        channel_waitq_remove(&c->recvq, *w);
        pthread_mutex_unlock(&c->lock);
        sem_destroy(sem);
        free(*w);
        return CH_CONTINUE_;
    }
    if (ch_load_acq_(&c->refc) == 0) {
        channel_waitq_remove(&c->recvq, *w);
        pthread_mutex_unlock(&c->lock);
        sem_destroy(sem);
        free(*w);
        return CH_CLOSED;
    }
    pthread_mutex_unlock(&c->lock);
    return CH_EMPTY;
}

inline int
channel_buf_recv_(channel_buf_ *c, void *elt) {
    for ( ; ; ) {
        sem_t sem;
        channel_waiter_ *w;
        int rc = channel_buf_recv_or_add_waiter_(c, elt, &sem, &w);
        if (rc == CH_CONTINUE_) {
            continue;
        } else if (rc != CH_EMPTY) {
            return rc;
        }

        sem_wait(&sem);
        sem_destroy(&sem);
        free(w);
    }
}

inline int
channel_unbuf_recv_or_add_waiter_(
    channel_unbuf_ *c, void *elt, sem_t *sem, channel_waiter_ **w
) {
    if (ch_load_acq_(&c->refc) == 0) {
        return CH_CLOSED;
    }

    pthread_mutex_lock(&c->lock);
    if (ch_load_acq_(&c->refc) == 0) {
        pthread_mutex_unlock(&c->lock);
        return CH_CLOSED;
    }
    channel_waiter_unbuf_ *w1 = &channel_waitq_shift_(&c->sendq)->unbuf;
    if (w1) {
        pthread_mutex_unlock(&c->lock);
        if (w1->sel_state) {
            uint32_t magic = CH_SEL_MAGIC_;
            if (!ch_cas_strong_(w1->sel_state, &magic, w1->sel_id)) {
                return CH_CONTINUE_;
            }
        }
        memcpy(elt, w1->elt, c->eltsize);
        sem_post(w1->sem);
        return CH_OK;
    }
    sem_init(sem, 0, 0);
    *w = channel_waitq_push_(&c->recvq, sem, NULL, CH_SEL_NIL_, elt);
    pthread_mutex_unlock(&c->lock);
    return CH_EMPTY;
}

inline int
channel_unbuf_recv_(channel_unbuf_ *c, void *elt) {
    for ( ; ; ) {
        sem_t sem;
        channel_waiter_ *w;
        int rc = channel_unbuf_recv_or_add_waiter_(c, elt, &sem, &w);
        if (rc == CH_CONTINUE_) {
            continue;
        } else if (rc != CH_EMPTY) {
            return rc;
        }

        sem_wait(&sem);
        rc = w->unbuf.closed ? CH_CLOSED : CH_OK;
        sem_destroy(&sem);
        free(w);
        return rc;
    }
}

/* Blocking receives block on buffered channels if the buffer is empty and
 * block on unbuffered channels if there is no waiting sender. Returns `CH_OK`
 * on success or `CH_CLOSED` if the channel is closed. */
inline int
channel_recv(channel *c, void *elt, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize);
    if (c->hdr.cap > 0) {
        return channel_buf_recv_(&c->buf, elt);
    }
    return channel_unbuf_recv_(&c->unbuf, elt);
}

/* Unfortunately we have to use `CLOCK_REALTIME` as there is no way to use
 * `CLOCK_MONOTIME` with `pthread_mutex_timedlock` and `sem_timedwait`. */
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

inline int
channel_buf_timedsend_(channel_buf_ *c, void *elt, ch_timespec_ *timeout) {
    for ( ; ; ) {
        sem_t sem;
        channel_waiter_ *w;
        int rc = channel_buf_send_or_add_waiter_(c, elt, &sem, &w);
        if (rc == CH_CONTINUE_) {
            continue;
        } else if (rc != CH_FULL) {
            return rc;
        }

        if (sem_timedwait(&sem, timeout) != 0) { // Not == ETIMEDOUT b/c Apple
            pthread_mutex_lock(&c->lock);
            if (!channel_waitq_remove(&c->sendq, w)) {
                pthread_mutex_unlock(&c->lock);
                int rc = channel_buf_trysend_(c, elt);
                if (rc != CH_FULL) {
                    return rc;
                }
            }
            pthread_mutex_unlock(&c->lock);
            sem_destroy(&sem);
            free(w);
            return CH_FULL | CH_TIMEDOUT;
        }
        sem_destroy(&sem);
        free(w);
    }
}

inline int
channel_unbuf_timedsend_(channel_unbuf_ *c, void *elt, ch_timespec_ *timeout) {
    for ( ; ; ) {
        sem_t sem;
        channel_waiter_ *w;
        int rc = channel_unbuf_send_or_add_waiter_(c, elt, &sem, &w);
        if (rc == CH_CONTINUE_) {
            continue;
        } else if (rc != CH_FULL) {
            return rc;
        }
        if (ch_load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }

        if (sem_timedwait(&sem, timeout) != 0) {
            pthread_mutex_lock(&c->lock);
            bool onqueue = channel_waitq_remove(&c->sendq, w);
            pthread_mutex_unlock(&c->lock);
            rc =
                w->unbuf.closed ? CH_CLOSED :
                onqueue ?  CH_FULL | CH_TIMEDOUT :
                CH_OK;
        } else {
            rc = w->unbuf.closed ? CH_CLOSED : CH_OK;
        }
        sem_destroy(&sem);
        free(w);
        return rc;
    }
}

/* Timed sends fail on buffered channels if the channel is full for the
 * duration of the timeout and fail on unbuffered channels if there is no
 * waiting receiver for the duration of the timeout. Returns `CH_OK` on
 * success, `CH_FULL | CH_TIMEDOUT` on failure, or `CH_CLOSED` if the channel
 * is closed.
 *
 * XXX Completely untested. */
inline int
channel_timedsend(channel *c, void *elt, uint64_t timeout, size_t eltsize) {
    ch_assert_(eltsize == c->hdr.eltsize);
    ch_timespec_ t = channel_add_timeout_(timeout);
    if (c->hdr.cap > 0) {
        return channel_buf_timedsend_(&c->buf, elt, &t);
    }
    return channel_unbuf_timedsend_(&c->unbuf, elt, &t);
}

inline int
channel_buf_timedrecv_(channel_buf_ *c, void *elt, ch_timespec_ *timeout) {
    for ( ; ; ) {
        sem_t sem;
        channel_waiter_ *w;
        int rc = channel_buf_recv_or_add_waiter_(c, elt, &sem, &w);
        if (rc == CH_CONTINUE_) {
            continue;
        } else if (rc != CH_EMPTY) {
            return rc;
        }

        if (sem_timedwait(&sem, timeout) != 0) {
            pthread_mutex_lock(&c->lock);
            if (!channel_waitq_remove(&c->recvq, w)) {
                pthread_mutex_unlock(&c->lock);
                int rc = channel_buf_tryrecv_(c, elt);
                if (rc != CH_EMPTY) {
                    return rc;
                }
            }
            pthread_mutex_unlock(&c->lock);
            sem_destroy(&sem);
            free(w);
            return CH_EMPTY | CH_TIMEDOUT;
        }
        sem_destroy(&sem);
        free(w);
    }
}

inline int
channel_unbuf_timedrecv_(channel_unbuf_ *c, void *elt, ch_timespec_ *timeout) {
    for ( ; ; ) {
        sem_t sem;
        channel_waiter_ *w;
        int rc = channel_unbuf_recv_or_add_waiter_(c, elt, &sem, &w);
        if (rc == CH_CONTINUE_) {
            continue;
        } else if (rc != CH_EMPTY) {
            return rc;
        }

        if (sem_timedwait(&sem, timeout) != 0) {
            pthread_mutex_lock(&c->lock);
            bool onqueue = channel_waitq_remove(&c->recvq, w);
            pthread_mutex_unlock(&c->lock);
            rc =
                w->unbuf.closed ? CH_CLOSED :
                onqueue ?  CH_EMPTY | CH_TIMEDOUT :
                CH_OK;
        } else {
            rc = w->unbuf.closed ? CH_CLOSED : CH_OK;
        }
        sem_destroy(&sem);
        free(w);
        return rc;
    }
}

/* Timed receives fail on buffered channels if the channel is empty for the
 * duration of the timeout and fail on unbuffered channels if there is no
 * waiting sender for the duration of the timeout. Returns `CH_OK` on success,
 * `CH_EMPTY | CH_TIMEDOUT` on failure, or `CH_CLOSED` if the channel is
 * closed.
 *
 * XXX Completely untested. */
inline int
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
    sem_init(&s->sem, 0, 0);
    return s;
}

/* Deallocates all resources associated with the channel set and returns
 * `NULL`. */
inline channel_set *
channel_set_drop(channel_set *s) {
    sem_destroy(&s->sem);
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
    s->arr[s->len] = (channel_case_){c, &s->sem, elt, NULL, op};
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

inline bool
channel_select_test_all_(
    channel_set *s, uint32_t *selrc, uint32_t offset
) {
    for (uint32_t i = 0; i < s->len; i++) {
        channel_case_ cc = s->arr[(i + offset) % s->len];
        if (cc.op == CH_NOOP) {
            continue;
        }

        int rc;
        if (cc.op == CH_SEND) {
            rc = channel_trysend(cc.c, cc.elt, cc.c->hdr.eltsize);
        } else {
            rc = channel_tryrecv(cc.c, cc.elt, cc.c->hdr.eltsize);
        }
        if (rc == CH_OK) {
            *selrc = (i + offset) % s->len;
            return true;
        }
    }
    return false;
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
        if (u.u32.lap == ch_load_acq_(ch_cell_lap_(cell))) {
            return true;
        }
        return false;
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
        pthread_mutex_lock(&cc->c->hdr.lock);
        cc->waiter = channel_waitq_push_(
            waitq, &s->sem, state, (i + offset) % s->len, elt);
        if (channel_select_ready_(cc->c, cc->op)) {
            channel_waitq_remove(waitq, cc->waiter);
            pthread_mutex_unlock(&cc->c->hdr.lock);
            free(cc->waiter);
            cc->waiter = NULL;
            return CH_SEL_RC_READY_;
        }
        pthread_mutex_unlock(&cc->c->hdr.lock);
    }

    if (closedc == s->len) {
        return CH_SEL_RC_CLOSED_;
    }
    return CH_SEL_RC_WAIT_;
}

inline void
channel_select_remove_waiters_(channel_set *s) {
    for (uint32_t i = 0; i < s->len; i++) {
        channel_case_ *cc = s->arr + i;
        if (cc->op == CH_NOOP ||
                ch_load_acq_(&cc->c->hdr.refc) == 0 ||
                !cc->waiter) {
            continue;
        }
        channel_waiter_ *_Atomic *waitq = cc->op == CH_SEND ?
                &cc->c->hdr.sendq : &cc->c->hdr.recvq;
        pthread_mutex_lock(&cc->c->hdr.lock);
        channel_waitq_remove(waitq, cc->waiter);
        free(cc->waiter);
        pthread_mutex_unlock(&cc->c->hdr.lock);
        cc->waiter = NULL;
    }
}

/* Randomly performs the registered operation on one channel in the channel set
 * that is ready to perform that operation. Blocks if no channel is ready.
 * Returns the id of the channel successfully completes its operation or
 * `CH_SEL_CLOSED` if all channels are closed or have been registered with
 * `CH_NOOP`.
 *
 * XXX Completely untested. */
inline uint32_t
channel_select(channel_set *s, uint64_t timeout) {
    uint32_t offset = rand() % s->len;
    ch_timespec_ t;
    if (timeout > 0) {
        t = channel_add_timeout_(timeout);
    }
    for ( ; ; ) {
        uint32_t rc;
        if (channel_select_test_all_(s, &rc, offset)) {
            return rc;
        }

        _Atomic uint32_t state;
        uint32_t magic = CH_SEL_MAGIC_;
        bool timedout = false;
        ch_store_rel_(&state, magic); // Could be store_rlx_?
        switch (channel_select_ready_or_wait_(s, &state, offset)) {
        case CH_SEL_RC_CLOSED_:
            return CH_SEL_CLOSED;
        case CH_SEL_RC_READY_:
            break;
        case CH_SEL_RC_WAIT_:
            if (timeout > 0) {
                if (sem_timedwait(&s->sem, &t) != 0) {
                    timedout = true;
                }
            } else {
                sem_wait(&s->sem);
            }
        }

        ch_cas_strong_(&state, &magic, CH_SEL_NIL_);
        channel_select_remove_waiters_(s);
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
        if (timedout) { // Some assumptions here (^:
            return CH_SEL_TIMEDOUT;
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
