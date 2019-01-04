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
#if !defined(__BYTE_ORDER__) || (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error "The changes for big endian are trivial but I need a box to test on."
#endif
_Static_assert(ATOMIC_LLONG_LOCK_FREE == 2, "I mean, it'll work but...");

/* ------------------------------- Interface ------------------------------- */
#define CHANNEL_H_VERSION 0l // 0.0.0
#define CHANNEL_H_GNU

/* Each individual channel type must be defined before use. Pointer types must
 * be wrapped with `ptr` instead of using `*` as type names are created via
 * token pasting. E.g. `CHANNEL_DEF_PTR(int);` defines the type of channels of
 * pointers to integers and `channel(ptr(int)) c;` declares one such channel.
 */
#define channel(T) channel_paste_(T)
#define channel_paste_(T) channel##T##_
#ifndef MINIMAL_
#define MINIMAL_
#define ptr(T) ptr_paste_(T)
#define ptr_paste_(T) ptr_##T##_
#define PTR_DEF(T) typedef T *ptr(T)
#endif

#define CHANNEL_DEF(T) \
    typedef union channel(T) { \
        channel_ hdr; \
        T *phantom; \
    } channel(T)
#define CHANNEL_DEF_PTR(T) \
    PTR_DEF(T); \
    CHANNEL_DEF(ptr(T))

typedef union channel_ channel_;
typedef struct channel_set channel_set;

/* Return codes */
typedef uint32_t channel_rc;
#define CH_OK 0
#define CH_WBLOCK (UINT32_MAX - 1)
#define CH_CLOSED UINT32_MAX

/* Op codes */
typedef enum channel_op {
    CH_NOOP,
    CH_SEND,
    CH_RECV,
} channel_op;

/* Exported "functions" */
#define ch_make(T, cap) ((channel(T) *)channel_make(sizeof(T), cap))
#define ch_dup(c) ch_dup_(c, __COUNTER__)
#define ch_drop(c) channel_drop(&(c)->hdr)
#define ch_open(c) ch_open_(c, __COUNTER__)
#define ch_close(c) ch_close_(c, __COUNTER__)

#define ch_send(c, msg) ( \
    (void)sizeof((*c->phantom = msg)), \
    channel_send(&(c)->hdr, &(msg), sizeof(msg)) \
)
#define ch_trysend(c, msg) ( \
    (void)sizeof((*c->phantom = msg)), \
    channel_trysend(&(c)->hdr, &(msg), sizeof(msg)) \
)
#define ch_timedsend(c, msg, timeout) ( \
    (void)sizeof((*c->phantom = msg)), \
    channel_timedsend(&(c)->hdr, &(msg), timeout, sizeof(msg)) \
)
#define ch_forcesend(c, msg) ( \
    (void)sizeof((*c->phantom = msg)), \
    channel_forcesend(&(c)->hdr, &(msg), sizeof(msg)) \
)

#define ch_recv(c, msg) ( \
    (void)sizeof((*c->phantom = msg)), \
    channel_recv(&(c)->hdr, &(msg), sizeof(msg)) \
)
#define ch_tryrecv(c, msg) ( \
    (void)sizeof((*c->phantom = msg)), \
    channel_tryrecv(&(c)->hdr, &(msg), sizeof(msg)) \
)
#define ch_timedrecv(c, msg, timeout) ( \
    (void)sizeof((*c->phantom = msg)), \
    channel_timedrecv(&(c)->hdr, &(msg), timeout, sizeof(msg)) \
)

#define ch_set_make(cap) channel_set_make(cap)
#define ch_set_drop(set) channel_set_drop(set)
#define ch_set_add(set, c, op, msg) ( \
    (void)sizeof((*c->phantom = msg)), \
    channel_set_add(set, &(c)->hdr, op, &(msg), sizeof(msg)) \
)
#define ch_set_rereg(set, id, op, msg) ( \
    (void)sizeof((*c->phantom = msg)), \
    channel_set_rereg(set, id, op, &(msg), sizeof(msg)) \
)

#define ch_select(set) channel_select(set, UINT64_MAX)
#define ch_tryselect(set) channel_select(set, 0)
#define ch_timedselect(set, timeout) channel_select(set, timeout)

/* These declarations must be present in exactly one compilation unit. */
#define CHANNEL_EXTERN_DECL \
    CHANNEL_SEM_WAIT_DECL_ \
    CHANNEL_SEM_TIMEDWAIT_DECL_ \
    extern inline void channel_assert_( \
        const char *, unsigned, const char *) __attribute__((noreturn)); \
    extern inline void *channel_make(uint32_t, uint32_t); \
    extern inline void *channel_drop(channel_ *); \
    extern inline void channel_waitq_push_( \
        channel_waiter_ *_Atomic *, \
        channel_waiter_ *waiter); \
    extern inline channel_waiter_ *channel_waitq_shift_( \
        channel_waiter_ *_Atomic *); \
    extern inline bool channel_waitq_remove_( \
        channel_waiter_ *_Atomic *, channel_waiter_ *); \
    extern inline void channel_close(channel_ *); \
    extern inline void channel_buf_waitq_shift_( \
        channel_waiter_ *_Atomic *, ch_mutex_ *); \
    extern inline channel_rc channel_buf_trysend_(channel_buf_ *, void *); \
    extern inline channel_rc channel_buf_tryrecv_(channel_buf_ *, void *); \
    extern inline channel_rc channel_unbuf_try_( \
        channel_unbuf_ *, void *, channel_waiter_ *_Atomic *); \
    extern inline channel_rc channel_buf_send_or_add_waiter_( \
        channel_buf_ *, void *, channel_waiter_buf_ *); \
    extern inline channel_rc channel_buf_recv_or_add_waiter_( \
        channel_buf_ *, void *, channel_waiter_buf_ *); \
    extern inline channel_rc channel_buf_send_( \
        channel_buf_ *, void *, ch_timespec_ *); \
    extern inline channel_rc channel_buf_recv_( \
        channel_buf_ *, void *, ch_timespec_ *); \
    extern inline channel_rc channel_unbuf_rendez_or_add_waiter_( \
        channel_unbuf_ *, void *, channel_waiter_unbuf_ *, channel_op); \
    extern inline channel_rc channel_unbuf_rendez_( \
        channel_unbuf_ *, void *, ch_timespec_ *, channel_op); \
    extern inline ch_timespec_ channel_add_timeout_(uint64_t); \
    extern inline channel_rc channel_send(channel_ *, void *, uint32_t); \
    extern inline channel_rc channel_recv(channel_ *, void *, uint32_t); \
    extern inline channel_rc channel_trysend(channel_ *, void *, uint32_t); \
    extern inline channel_rc channel_tryrecv(channel_ *, void *, uint32_t); \
    extern inline channel_rc channel_timedsend( \
        channel_ *, void *, uint64_t, uint32_t); \
    extern inline channel_rc channel_timedrecv( \
        channel_ *, void *, uint64_t, uint32_t); \
    extern inline channel_rc channel_forcesend(channel_ *, void *, uint32_t); \
    extern inline channel_set *channel_set_make(uint32_t); \
    extern inline channel_set *channel_set_drop(channel_set *); \
    extern inline uint32_t channel_set_add( \
        channel_set *, channel_ *, channel_op, void *, uint32_t); \
    extern inline void channel_set_rereg( \
        channel_set *, uint32_t, channel_op, void *, uint32_t); \
    extern inline uint32_t channel_select_test_all_(channel_set *, uint32_t); \
    extern inline bool channel_select_ready_(channel_ *, channel_op); \
    extern inline channel_select_rc_ channel_select_ready_or_wait_( \
        channel_set *, _Atomic uint32_t *, uint32_t); \
    extern inline void channel_select_remove_waiters_( \
        channel_set *, uint32_t); \
    extern inline uint32_t channel_select(channel_set *, uint64_t)

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
        abort();
    }
    return 0;
}

inline int
channel_sem_timedwait_(sem_t *sem, const struct timespec *ts) {
    int rc;
    while ((rc = sem_timedwait(sem, ts)) != 0) {
        switch (errno) {
        case ETIMEDOUT: return rc;
        case EINTR:
            errno = 0;
            continue;
        default: abort();
        }
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
    void *msg;
} channel_waiter_unbuf_;

typedef union channel_waiter_ {
    channel_waiter_hdr_ hdr;
    channel_waiter_buf_ buf;
    channel_waiter_unbuf_ unbuf;
} channel_waiter_;

typedef struct channel_hdr_ {
    uint32_t cap, msgsize;
    _Atomic uint32_t openc, refc;
    channel_waiter_ *_Atomic sendq, *_Atomic recvq;
    ch_mutex_ lock;
} channel_hdr_;

/* If C had generics, the cell struct would be defined as follows, except it
 * isn't ever padded (TODO? might be a problem on some archs):
 * typedef struct channel_cell_<T> {
 *     _Atomic uint32_t lap;
 *     T msg;
 * } channel_cell_<T>; */
#define ch_cellsize_(msgsize) (sizeof(uint32_t) + msgsize)
#define ch_cell_lap_(cell) ((_Atomic uint32_t *)cell)
#define ch_cell_msg_(cell) (cell + sizeof(uint32_t))

typedef union channel_aun64_ {
    _Atomic uint64_t u64;
    struct {
        _Atomic uint32_t index, lap;
    } u32;
} channel_aun64_;

typedef union channel_un64_ {
    uint64_t u64;
    struct {
        uint32_t index, lap;
    } u32;
} channel_un64_;

typedef struct channel_buf_ {
    uint32_t cap, msgsize;
    _Atomic uint32_t openc, refc;
    channel_waiter_ *_Atomic sendq, *_Atomic recvq;
    ch_mutex_ lock;
    channel_aun64_ write;
    char pad[64 - sizeof(channel_aun64_)]; // Cache line padding
    channel_aun64_ read;
    char pad1[64 - sizeof(channel_aun64_)];
    char buf[]; // channel_cell_<T> buf[];
} channel_buf_;

/* Unbuffered channels currently only use the fields in the shared header. */
typedef struct channel_hdr_ channel_unbuf_;

union channel_ {
    channel_hdr_ hdr;
    channel_buf_ buf;
    channel_unbuf_ unbuf;
};

typedef struct channel_case_ {
    channel_ *c;
    void *msg;
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

#define CH_SEL_NIL_ CH_WBLOCK
#define CH_SEL_MAGIC_ CH_CLOSED

#define ch_load_rlx_(obj) atomic_load_explicit(obj, memory_order_relaxed)
#define ch_load_acq_(obj) atomic_load_explicit(obj, memory_order_acquire)
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

#define ch_sym_(sym, id) CH_##sym##id##_

/* Increments the reference count of the channel and returns the channel. */
#define ch_dup_(c, id) __extension__ ({ \
    __auto_type ch_sym_(c_, id) = c; \
    uint32_t prev = ch_faa_acq_rel_(&ch_sym_(c_, id)->hdr.hdr.refc, 1); \
    ch_assert_(0 < prev && prev < UINT32_MAX); \
    ch_sym_(c_, id); \
})

/* Increments the open count of the channel and returns the channel. Additional
 * opens can be useful in scenarios with multiple producers independently
 * closing the channel. */
#define ch_open_(c, id) __extension__ ({ \
    __auto_type ch_sym_(c_, id) = c; \
    uint32_t prev = ch_faa_acq_rel_(&ch_sym_(c_, id)->hdr.hdr.openc, 1); \
    ch_assert_(0 < prev && prev < UINT32_MAX); \
    ch_sym_(c_, id); \
})

#define ch_close_(c, id) __extension__ ({ \
    __auto_type ch_sym_(c_, id) = c; \
    channel_close(&ch_sym_(c_, id)->hdr); \
    ch_sym_(c_, id); \
})

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
inline void *
channel_make(uint32_t msgsize, uint32_t cap) {
    channel_ *c;
    if (cap > 0) {
        ch_assert_(cap <= SIZE_MAX / msgsize);
        ch_assert_((c = calloc(
            1, offsetof(channel_buf_, buf) + (cap * ch_cellsize_(msgsize)))));
        ch_store_rlx_(&c->buf.read.u32.lap, 1);
    } else {
        ch_assert_((c = calloc(1, sizeof(c->unbuf))));
    }
    c->hdr.cap = cap;
    c->hdr.msgsize = msgsize;
    ch_store_rlx_(&c->hdr.openc, 1);
    ch_store_rlx_(&c->hdr.refc, 1);
    ch_mutex_init_(c->hdr.lock);
    return c;
}

/* Deallocates all resources associated with the channel if the caller has the
 * last reference. Decrements the reference count otherwise. Returns `NULL`. */
inline void *
channel_drop(channel_ *c) {
    uint32_t refc = ch_fas_acq_rel_(&c->hdr.refc, 1);
    if (refc == 1) {
        ch_assert_(ch_mutex_destroy_(&c->hdr.lock) == 0);
        free(c);
    } else {
        ch_assert_(refc != 0);
    }
    return NULL;
}

inline void
channel_waitq_push_(channel_waiter_ *_Atomic *waitq, channel_waiter_ *waiter) {
    channel_waiter_ *wq = ch_load_rlx_(waitq);
    if (!wq) {
        waiter->hdr.prev = waiter;
        ch_store_seq_cst_(waitq, waiter);
    } else {
        waiter->hdr.prev = wq->hdr.prev;
        waiter->hdr.prev->hdr.next = waiter;
        wq->hdr.prev = waiter;
    }
    waiter->hdr.next = NULL;
}

inline channel_waiter_ *
channel_waitq_shift_(channel_waiter_ *_Atomic *waitq) {
    channel_waiter_ *w = ch_load_rlx_(waitq);
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

inline bool
channel_waitq_remove_(channel_waiter_ *_Atomic *waitq, channel_waiter_ *w) {
    if (!w->hdr.prev) { // Already shifted off the front
        return false;
    }
    if (w->hdr.prev == w) { // `w` is the only element
        ch_store_seq_cst_(waitq, NULL);
        return true;
    }

    if (w->hdr.prev->hdr.next) {
        w->hdr.prev->hdr.next = w->hdr.next;
    } else {
        ch_store_seq_cst_(waitq, w->hdr.next);
    }
    if (w->hdr.next) {
        w->hdr.next->hdr.prev = w->hdr.prev;
    } else {
        ch_load_rlx_(waitq)->hdr.prev = w->hdr.prev;
    }
    return true;
}

/* Closes the channel if the caller has the last open handle to the channel.
 * Decrements the open count otherwise. Returns the channel. */
inline void
channel_close(channel_ *c) {
    uint32_t openc = ch_fas_acq_rel_(&c->hdr.openc, 1);
    if (openc != 1) {
        ch_assert_(openc != 0);
        return;
    }

    ch_mutex_lock_(&c->hdr.lock);
    if (c->hdr.cap > 0) {
        channel_waiter_buf_ *w;
        /* UBSan complains about union member accesses when the pointer is
         * `NULL` but I don't think I care since we aren't actually
         * dereferencing the pointer. */
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
channel_buf_trysend_(channel_buf_ *c, void *msg) {
    for (int i = 0; ; ) {
        if (ch_load_acq_(&c->openc) == 0) {
            return CH_CLOSED;
        }

        channel_un64_ write = {ch_load_acq_(&c->write.u64)};
        char *cell = c->buf + (write.u32.index * ch_cellsize_(c->msgsize));
        uint32_t lap = ch_load_acq_(ch_cell_lap_(cell));
        if (write.u32.lap != lap) {
            if (i++ < 4) { // Improves benchmarks; harmful in normal usage?
                sched_yield();
                continue;
            }
            return CH_WBLOCK;
        }

        uint64_t write1 = write.u32.index + 1 < c->cap ?
                write.u64 + 1 : (uint64_t)(write.u32.lap + 2) << 32;
        if (ch_cas_weak_(&c->write.u64, &write.u64, write1)) {
            memcpy(ch_cell_msg_(cell), msg, c->msgsize);
            ch_store_rel_(ch_cell_lap_(cell), lap + 1);
            channel_buf_waitq_shift_(&c->recvq, &c->lock);
            return CH_OK;
        }
    }
}

inline channel_rc
channel_buf_tryrecv_(channel_buf_ *c, void *msg) {
    for (int i = 0; ; ) {
        channel_un64_ read = {ch_load_acq_(&c->read.u64)};
        char *cell = c->buf + (read.u32.index * ch_cellsize_(c->msgsize));
        uint32_t lap = ch_load_acq_(ch_cell_lap_(cell));
        if (read.u32.lap != lap) {
            if (ch_load_acq_(&c->openc) == 0) {
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
            memcpy(msg, ch_cell_msg_(cell), c->msgsize);
            ch_store_rel_(ch_cell_lap_(cell), lap + 1);
            channel_buf_waitq_shift_(&c->sendq, &c->lock);
            return CH_OK;
        }
    }
}

inline channel_rc
channel_unbuf_try_(
    channel_unbuf_ *c, void *msg, channel_waiter_ *_Atomic *waitq
) {
    for ( ; ; ) {
        if (ch_load_acq_(&c->openc) == 0) {
            return CH_CLOSED;
        }
        if (!ch_load_acq_(waitq)) {
            return CH_WBLOCK;
        }

        ch_mutex_lock_(&c->lock);
        if (ch_load_acq_(&c->openc) == 0) {
            ch_mutex_unlock_(&c->lock);
            return CH_CLOSED;
        }
        channel_waiter_unbuf_ *w = &channel_waitq_shift_(waitq)->unbuf;
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
        if (waitq == &c->recvq) {
            memcpy(w->msg, msg, c->msgsize);
        } else {
            memcpy(msg, w->msg, c->msgsize);
        }
        ch_sem_post_(w->sem);
        return CH_OK;
    }
}

inline channel_rc
channel_buf_send_or_add_waiter_(
    channel_buf_ *c, void *msg, channel_waiter_buf_ *w
) {
    for ( ; ; ) {
        channel_rc rc = channel_buf_trysend_(c, msg);
        if (rc != CH_WBLOCK) {
            return rc;
        }

        ch_mutex_lock_(&c->lock);
        if (ch_load_acq_(&c->openc) == 0) {
            ch_mutex_unlock_(&c->lock);
            return CH_CLOSED;
        }
        /* TODO: Casts are evil. Figure out how to get rid of these. */
        channel_waitq_push_(&c->sendq, (channel_waiter_ *)w);
        channel_un64_ write = {ch_load_acq_(&c->write.u64)};
        char *cell = c->buf + (write.u32.index * ch_cellsize_(c->msgsize));
        if (write.u32.lap == ch_load_acq_(ch_cell_lap_(cell))) {
            channel_waitq_remove_(&c->sendq, (channel_waiter_ *)w);
            ch_mutex_unlock_(&c->lock);
            continue;
        }
        ch_mutex_unlock_(&c->lock);
        return CH_WBLOCK;
    }
}

inline channel_rc
channel_buf_recv_or_add_waiter_(
    channel_buf_ *c, void *msg, channel_waiter_buf_ *w
) {
    for ( ; ; ) {
        channel_rc rc = channel_buf_tryrecv_(c, msg);
        if (rc != CH_WBLOCK) {
            return rc;
        }

        ch_mutex_lock_(&c->lock);
        channel_waitq_push_(&c->recvq, (channel_waiter_ *)w);
        channel_un64_ read = {ch_load_acq_(&c->read.u64)};
        char *cell = c->buf + (read.u32.index * ch_cellsize_(c->msgsize));
        if (read.u32.lap == ch_load_acq_(ch_cell_lap_(cell))) {
            channel_waitq_remove_(&c->recvq, (channel_waiter_ *)w);
            ch_mutex_unlock_(&c->lock);
            continue;
        }
        if (ch_load_acq_(&c->openc) == 0) {
            channel_waitq_remove_(&c->recvq, (channel_waiter_ *)w);
            ch_mutex_unlock_(&c->lock);
            return CH_CLOSED;
        }
        ch_mutex_unlock_(&c->lock);
        return CH_WBLOCK;
    }
}

inline channel_rc
channel_buf_send_(channel_buf_ *c, void *msg, ch_timespec_ *timeout) {
    ch_sem_ sem;
    ch_sem_init_(&sem, 0, 0);
    channel_waiter_buf_ w = {.sem = &sem, .sel_id = CH_SEL_NIL_};

    for ( ; ; ) {
        channel_rc rc = channel_buf_send_or_add_waiter_(c, msg, &w);
        if (rc != CH_WBLOCK) {
            ch_sem_destroy_(&sem);
            return rc;
        }

        if (timeout == NULL) {
            ch_sem_wait_(&sem);
        } else if (ch_sem_timedwait_(&sem, timeout) != 0) { // != 0 due to OS X
            ch_mutex_lock_(&c->lock);
            bool onqueue =
                channel_waitq_remove_(&c->sendq, (channel_waiter_ *)&w);
            ch_mutex_unlock_(&c->lock);
            if (!onqueue) {
                ch_sem_wait_(w.sem);
                channel_rc rc = channel_buf_trysend_(c, msg);
                if (rc != CH_WBLOCK) {
                    ch_sem_destroy_(&sem);
                    return rc;
                }
            }
            ch_sem_destroy_(&sem);
            return CH_WBLOCK;
        }
    }
}

inline channel_rc
channel_buf_recv_(channel_buf_ *c, void *msg, ch_timespec_ *timeout) {
    ch_sem_ sem;
    ch_sem_init_(&sem, 0, 0);
    channel_waiter_buf_ w = {.sem = &sem, .sel_id = CH_SEL_NIL_};

    for ( ; ; ) {
        channel_rc rc = channel_buf_recv_or_add_waiter_(c, msg, &w);
        if (rc != CH_WBLOCK) {
            ch_sem_destroy_(&sem);
            return rc;
        }

        if (timeout == NULL) {
            ch_sem_wait_(&sem);
        } else if (ch_sem_timedwait_(&sem, timeout) != 0) {
            ch_mutex_lock_(&c->lock);
            bool onqueue =
                channel_waitq_remove_(&c->recvq, (channel_waiter_ *)&w);
            ch_mutex_unlock_(&c->lock);
            if (!onqueue) {
                ch_sem_wait_(w.sem);
                channel_rc rc = channel_buf_tryrecv_(c, msg);
                if (rc != CH_WBLOCK) {
                    ch_sem_destroy_(&sem);
                    return rc;
                }
            }
            ch_sem_destroy_(&sem);
            return CH_WBLOCK;
        }
    }
}

inline channel_rc
channel_unbuf_rendez_or_add_waiter_(
    channel_unbuf_ *c, void *msg, channel_waiter_unbuf_ *w, channel_op op
) {
    channel_waiter_ *_Atomic *shiftq, *_Atomic *pushq;
    if (op == CH_SEND) {
        shiftq = &c->recvq;
        pushq = &c->sendq;
    } else {
        shiftq = &c->sendq;
        pushq = &c->recvq;
    }

    for ( ; ; ) {
        if (ch_load_acq_(&c->openc) == 0) {
            return CH_CLOSED;
        }

        ch_mutex_lock_(&c->lock);
        if (ch_load_acq_(&c->openc) == 0) {
            ch_mutex_unlock_(&c->lock);
            return CH_CLOSED;
        }
        channel_waiter_unbuf_ *w1 = &channel_waitq_shift_(shiftq)->unbuf;
        if (w1) {
            ch_mutex_unlock_(&c->lock);
            if (w1->sel_state) {
                uint32_t magic = CH_SEL_MAGIC_;
                if (!ch_cas_strong_(w1->sel_state, &magic, w1->sel_id)) {
                    ch_store_rel_(&w1->ref, false);
                    continue;
                }
            }
            if (op == CH_SEND) {
                memcpy(w1->msg, msg, c->msgsize);
            } else {
                memcpy(msg, w1->msg, c->msgsize);
            }
            ch_sem_post_(w1->sem);
            return CH_OK;
        }
        channel_waitq_push_(pushq, (channel_waiter_ *)w);
        ch_mutex_unlock_(&c->lock);
        return CH_WBLOCK;
    }
}

inline channel_rc
channel_unbuf_rendez_(
    channel_unbuf_ *c, void *msg, ch_timespec_ *timeout, channel_op op
) {
    ch_sem_ sem;
    ch_sem_init_(&sem, 0, 0);
    channel_waiter_unbuf_ w = {.sem = &sem, .sel_id = CH_SEL_NIL_, .msg = msg};
    channel_rc rc = channel_unbuf_rendez_or_add_waiter_(c, msg, &w, op);
    if (rc != CH_WBLOCK) {
        ch_sem_destroy_(&sem);
        return rc;
    }

    if (timeout == NULL) {
        ch_sem_wait_(&sem);
    } else if (ch_sem_timedwait_(&sem, timeout) != 0) {
        ch_mutex_lock_(&c->lock);
        bool onqueue = channel_waitq_remove_(&c->sendq, (channel_waiter_ *)&w);
        ch_mutex_unlock_(&c->lock);
        if (onqueue) {
            ch_sem_destroy_(&sem);
            return CH_WBLOCK;
        }
        ch_sem_wait_(w.sem);
    }
    ch_sem_destroy_(&sem);
    return w.closed ? CH_CLOSED : CH_OK;
}

/* Unfortunately we have to use `CLOCK_REALTIME` as there is no way to use
 * `CLOCK_MONOTIME` with `ch_sem_timedwait_`. 
 *
 * TODO: Sanity check the timeout parameter. */
inline ch_timespec_
channel_add_timeout_(uint64_t timeout) {
#ifdef __APPLE__
    dispatch_time_t ts = dispatch_time(DISPATCH_TIME_NOW, timeout * 1000);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += (timeout % 1000000) * 1000;
    ts.tv_sec += (ts.tv_nsec / 1000000000) + (timeout / 1000000);
    ts.tv_nsec %= 1000000000;
#endif
    return ts;
}

/* Blocking sends block on buffered channels if the buffer is full and block on
 * unbuffered channels if there is no waiting receiver. Returns `CH_OK` on
 * success or `CH_CLOSED` if the channel is closed. */
inline channel_rc
channel_send(channel_ *c, void *msg, uint32_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    return c->hdr.cap > 0 ?
            channel_buf_send_(&c->buf, msg, NULL) :
            channel_unbuf_rendez_(&c->unbuf, msg, NULL, CH_SEND);
}

/* Blocking receives block on buffered channels if the buffer is empty and
 * block on unbuffered channels if there is no waiting sender. Returns `CH_OK`
 * on success or `CH_CLOSED` if the channel is closed. */
inline channel_rc
channel_recv(channel_ *c, void *msg, uint32_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    return c->hdr.cap > 0 ?
            channel_buf_recv_(&c->buf, msg, NULL) :
            channel_unbuf_rendez_(&c->unbuf, msg, NULL, CH_RECV);
}

/* Nonblocking sends fail on buffered channels if the channel is full and fail
 * on unbuffered channels if there is no waiting receiver. Returns `CH_OK` on
 * success, `CH_WBLOCK` on failure, or `CH_CLOSED` if the channel is closed. */
inline channel_rc
channel_trysend(channel_ *c, void *msg, uint32_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    return c->hdr.cap > 0 ?
            channel_buf_trysend_(&c->buf, msg) :
            channel_unbuf_try_(&c->unbuf, msg, &c->unbuf.recvq);
}

/* Nonblocking receives fail on buffered channels if the channel is empty and
 * fail on unbuffered channels if there is no waiting sender. Returns `CH_OK`
 * on success, `CH_WBLOCK` on failure, or `CH_CLOSED` if the channel is
 * closed. */
inline channel_rc
channel_tryrecv(channel_ *c, void *msg, uint32_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    return c->hdr.cap > 0 ?
            channel_buf_tryrecv_(&c->buf, msg) :
            channel_unbuf_try_(&c->unbuf, msg, &c->unbuf.sendq);
}

/* Timed sends fail on buffered channels if the channel is full for the
 * duration of the timeout and fail on unbuffered channels if there is no
 * waiting receiver for the duration of the timeout. The timeout is specified
 * in microseconds. Returns `CH_OK` on success, `CH_WBLOCK` on failure, or
 * `CH_CLOSED` if the channel is closed. */
inline channel_rc
channel_timedsend(channel_ *c, void *msg, uint64_t timeout, uint32_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    ch_timespec_ ts = channel_add_timeout_(timeout);
    return c->hdr.cap > 0 ?
            channel_buf_send_(&c->buf, msg, &ts) :
            channel_unbuf_rendez_(&c->unbuf, msg, &ts, CH_SEND);
}

/* Timed receives fail on buffered channels if the channel is empty for the
 * duration of the timeout and fail on unbuffered channels if there is no
 * waiting sender for the duration of the timeout. Returns `CH_OK` on success,
 * `CH_WBLOCK` on failure, or `CH_CLOSED` if the channel is closed. */
inline channel_rc
channel_timedrecv(channel_ *c, void *msg, uint64_t timeout, uint32_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    ch_timespec_ ts = channel_add_timeout_(timeout);
    return c->hdr.cap > 0 ?
            channel_buf_recv_(&c->buf, msg, &ts) :
            channel_unbuf_rendez_(&c->unbuf, msg, &ts, CH_RECV);
}

/* Forced sends to buffered channels do not block and overwrite the oldest
 * message if the buffer is full. It is not possible to force sends to
 * unbuffered channels. Returns `CH_OK` on success or `CH_CLOSED` if the
 * channel is closed. */
inline channel_rc
channel_forcesend(channel_ *c, void *msg, uint32_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize && c->hdr.cap > 0);
    for (bool full = false; ; ) {
        if (ch_load_acq_(&c->buf.openc) == 0) {
            return CH_CLOSED;
        }

        channel_un64_ write = {ch_load_acq_(&c->buf.write.u64)};
        char *cell =
            c->buf.buf + (write.u32.index * ch_cellsize_(c->buf.msgsize));
        uint32_t lap = ch_load_acq_(ch_cell_lap_(cell));
        if (write.u32.lap != lap) {
            full = true;
            channel_un64_ read = {ch_load_acq_(&c->buf.read.u64)};
            char *cell1 =
                c->buf.buf + (read.u32.index * ch_cellsize_(c->buf.msgsize));
            uint32_t lap1 = ch_load_acq_(ch_cell_lap_(cell1));
            if (read.u32.lap != lap1) {
                continue;
            }

            uint64_t read1 = read.u32.index + 1 < c->buf.cap ?
                    read.u64 + 1 : (uint64_t)(read.u32.lap + 2) << 32;
            if (ch_cas_weak_(&c->buf.read.u64, &read.u64, read1)) {
                ch_store_rel_(ch_cell_lap_(cell1), lap1 + 1);
            }
            continue;
        }

        uint64_t write1 = write.u32.index + 1 < c->buf.cap ?
                write.u64 + 1 : (uint64_t)(write.u32.lap + 2) << 32;
        if (ch_cas_weak_(&c->buf.write.u64, &write.u64, write1)) {
            memcpy(ch_cell_msg_(cell), msg, c->buf.msgsize);
            ch_store_rel_(ch_cell_lap_(cell), lap + 1);
            channel_buf_waitq_shift_(&c->buf.recvq, &c->buf.lock);
            return full ? CH_WBLOCK : CH_OK;
        }
    }
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
    channel_set *s, channel_ *c, channel_op op, void *msg, uint32_t msgsize
) {
    if (s->len == s->cap) {
        ch_assert_((
            s->arr = realloc(s->arr, (s->cap *= 2) * sizeof(s->arr[0]))));
    }
    ch_assert_(msgsize == c->hdr.msgsize &&
        s->len < CH_SEL_NIL_ &&
        s->len < RAND_MAX);
    s->arr[s->len] = (channel_case_){c, msg, NULL, op};
    return s->len++;
}

/* Change the registered op or element of the specified channel in the channel
 * set. */
inline void
channel_set_rereg(
    channel_set *s, uint32_t id, channel_op op, void *msg, uint32_t msgsize
) {
    ch_assert_(msgsize == s->arr[id].c->hdr.msgsize);
    s->arr[id].op = op;
    s->arr[id].msg = msg;
}

inline uint32_t
channel_select_test_all_(channel_set *s, uint32_t offset) {
    for (uint32_t i = 0; i < s->len; i++) {
        channel_case_ cc = s->arr[(i + offset) % s->len];
        if ((cc.op == CH_SEND &&
                channel_trysend(cc.c, cc.msg, cc.c->hdr.msgsize) == CH_OK) ||
            (cc.op == CH_RECV &&
                channel_tryrecv(cc.c, cc.msg, cc.c->hdr.msgsize) == CH_OK)) {
            return (i + offset) % s->len;
        }
    }
    return CH_SEL_NIL_;
}

inline bool
channel_select_ready_(channel_ *c, channel_op op) {
    if (c->hdr.cap > 0) {
        channel_un64_ u;
        if (op == CH_SEND) {
            u.u64 = ch_load_acq_(&c->buf.write.u64);
        } else {
            u.u64 = ch_load_acq_(&c->buf.read.u64);
        }
        char *cell = c->buf.buf + (u.u32.index * ch_cellsize_(c->buf.msgsize));
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
        if (cc->op == CH_NOOP || ch_load_acq_(&cc->c->hdr.openc) == 0) {
            closedc++;
            continue;
        }

        if (cc->c->hdr.cap > 0) {
            ch_assert_((cc->waiter = calloc(1, sizeof(cc->waiter->buf))));
        } else {
            ch_assert_((cc->waiter = calloc(1, sizeof(cc->waiter->unbuf))));
            cc->waiter->unbuf.msg = cc->msg;
        }
        cc->waiter->hdr.sem = &s->sem;
        cc->waiter->hdr.sel_state = state;
        cc->waiter->hdr.sel_id = (i + offset) % s->len;
        channel_waiter_ *_Atomic *waitq = cc->op == CH_SEND ?
                &cc->c->hdr.sendq : &cc->c->hdr.recvq;
        ch_mutex_lock_(&cc->c->hdr.lock);
        channel_waitq_push_(waitq, cc->waiter);
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
            ch_load_acq_(&cc->c->hdr.openc) == 0 ||
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
 * that is ready to perform that operation. Blocks if no channel is ready. A
 * timeout in microseconds may be optionally specified. Returns the id of the
 * channel successfully completes its operation, `CH_CLOSED` if all channels
 * are closed or have been registered with `CH_NOOP`, or `CH_WBLOCK` if no
 * operation successfully completes before the timeout. */
inline uint32_t
channel_select(channel_set *s, uint64_t timeout) {
    uint32_t offset = rand() % s->len;
    ch_timespec_ ts;
    if (timeout == 0) {
        return channel_select_test_all_(s, offset);
    }
    if (timeout != UINT64_MAX) {
        ts = channel_add_timeout_(timeout);
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
        case CH_SEL_RC_CLOSED_: return CH_CLOSED;
        case CH_SEL_RC_READY_:
            ch_cas_strong_(&state, &magic, CH_SEL_NIL_);
            if (state < CH_SEL_NIL_) {
                ch_sem_wait_(&s->sem);
            }
            break;
        case CH_SEL_RC_WAIT_:
            if (timeout == UINT64_MAX) {
                ch_sem_wait_(&s->sem);
            } else if (ch_sem_timedwait_(&s->sem, &ts) != 0) {
                timedout = true;
                ch_cas_strong_(&state, &magic, CH_SEL_NIL_);
                if (state < CH_SEL_NIL_) {
                    ch_sem_wait_(&s->sem);
                }
                break;
            }
            ch_cas_strong_(&state, &magic, CH_SEL_NIL_);
        }

        channel_select_remove_waiters_(s, magic);
        if (state < CH_SEL_NIL_) {
            channel_case_ cc = s->arr[state];
            if (cc.c->hdr.cap == 0 ||
                (cc.op == CH_SEND && channel_buf_trysend_(
                    &cc.c->buf, cc.msg) == CH_OK) ||
                (cc.op == CH_RECV && channel_buf_tryrecv_(
                    &cc.c->buf, cc.msg) == CH_OK)) {
                return state;
            }
        }
        if (timedout) {
            return CH_WBLOCK;
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
