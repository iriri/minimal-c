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

/* ------------------------------- Interface ------------------------------- */
#define CHANNEL_H_VERSION 0l // 0.0.0

typedef union channel channel;

/* struct channel_case {
 *     channel *c;
 *     void *msg;
 *     channel_op op;
 *     ...
 * }; */
typedef struct channel_case channel_case;

/* Return codes */
typedef size_t channel_rc;
#define CH_OK 0
#define CH_WBLOCK (SIZE_MAX - 1)
#define CH_CLOSED SIZE_MAX

/* Op codes */
typedef enum channel_op {
    CH_NOOP,
    CH_SEND,
    CH_RECV,
} channel_op;

/* Exported "functions" */
#define ch_make(T, cap) channel_make(sizeof(T), cap)
#define ch_dup(c) channel_dup(c)
#define ch_drop(c) channel_drop(c)
#define ch_open(c) channel_open(c)
#define ch_close(c) channel_close(c)

#define ch_send(c, msg) channel_send(c, msg, sizeof(*msg))
#define ch_trysend(c, msg) channel_trysend(c, msg, sizeof(*msg))
#define ch_timedsend(c, msg, timeout) \
    channel_timedsend(c, msg, timeout, sizeof(*msg))

#define ch_recv(c, msg) channel_recv(c, msg, sizeof(*msg))
#define ch_tryrecv(c, msg) channel_tryrecv(c, msg, sizeof(*msg))
#define ch_timedrecv(c, msg, timeout) \
    channel_timedrecv(c, msg, timeout, sizeof(*msg))

#define ch_alt(cases, len) channel_alt(cases, len, UINT64_MAX)
#define ch_tryalt(cases, len) channel_tryalt(cases, len, rand())
#define ch_timedalt(cases, len, timeout) channel_alt(cases, len, timeout)

/* These declarations must be present in exactly one compilation unit. */
#define CHANNEL_EXTERN_DECL \
    CHANNEL_SEM_WAIT_DECL_ \
    CHANNEL_SEM_TIMEDWAIT_DECL_ \
    extern inline void channel_assert_( \
        const char *, unsigned, const char *) __attribute__((noreturn)); \
    extern inline channel *channel_make(size_t, size_t); \
    extern inline channel *channel_dup(channel *); \
    extern inline channel *channel_drop(channel *); \
    extern inline void channel_waitq_push_( \
        channel_waiter_root_ *, channel_waiter_ *waiter); \
    extern inline channel_waiter_ *channel_waitq_shift_( \
        channel_waiter_root_ *); \
    extern inline bool channel_waitq_remove_(channel_waiter_ *); \
    extern inline channel *channel_open(channel *); \
    extern inline channel *channel_close(channel *); \
    extern inline void channel_buf_waitq_shift_( \
        channel_waiter_root_ *, ch_mutex_ *); \
    extern inline channel_rc channel_buf_trysend_(channel_buf_ *, void *); \
    extern inline channel_rc channel_buf_tryrecv_(channel_buf_ *, void *); \
    extern inline channel_rc channel_unbuf_try_( \
        channel_unbuf_ *, void *, channel_waiter_root_ *); \
    extern inline channel_rc channel_buf_send_or_wait_( \
        channel_buf_ *, void *, channel_waiter_buf_ *); \
    extern inline channel_rc channel_buf_recv_or_wait_( \
        channel_buf_ *, void *, channel_waiter_buf_ *); \
    extern inline channel_rc channel_buf_send_( \
        channel_buf_ *, void *, ch_timespec_ *); \
    extern inline channel_rc channel_buf_recv_( \
        channel_buf_ *, void *, ch_timespec_ *); \
    extern inline channel_rc channel_unbuf_rendez_or_wait_( \
        channel_unbuf_ *, void *, channel_waiter_unbuf_ *, channel_op); \
    extern inline channel_rc channel_unbuf_rendez_( \
        channel_unbuf_ *, void *, ch_timespec_ *, channel_op); \
    extern inline ch_timespec_ channel_add_timeout_(uint64_t); \
    extern inline channel_rc channel_send(channel *, void *, size_t); \
    extern inline channel_rc channel_recv(channel *, void *, size_t); \
    extern inline channel_rc channel_trysend(channel *, void *, size_t); \
    extern inline channel_rc channel_tryrecv(channel *, void *, size_t); \
    extern inline channel_rc channel_timedsend( \
        channel *, void *, uint64_t, size_t); \
    extern inline channel_rc channel_timedrecv( \
        channel *, void *, uint64_t, size_t); \
    extern inline size_t channel_tryalt(channel_case[], size_t, size_t); \
    extern inline bool channel_alt_ready_(channel *, channel_op); \
    extern inline channel_alt_rc_ channel_alt_wait_( \
        channel_case[static 1], size_t, size_t, ch_sem_ *, _Atomic size_t *); \
    extern inline void channel_alt_remove_waiters_( \
        channel_case[static 1], size_t, size_t); \
    extern inline size_t channel_alt(channel_case[], size_t, uint64_t)

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
#define CHANNEL_SEM_WAIT_DECL_ extern inline void channel_sem_wait_(sem_t *);
#define CHANNEL_SEM_TIMEDWAIT_DECL_ \
    extern inline int channel_sem_timedwait_(sem_t *, const struct timespec *);

inline void
channel_sem_wait_(sem_t *sem) {
    while (sem_wait(sem) != 0) {
        if (errno != EINTR) {
            abort();
        }
        errno = 0;
    }
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
#elif defined __APPLE__ // OS X
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

typedef struct channel_waiter_root_ {
    union channel_waiter_ *_Atomic next, *_Atomic prev;
} channel_waiter_root_;

typedef struct channel_waiter_hdr_ {
    union channel_waiter_ *next, *prev;
    ch_sem_ *sem;
    _Atomic size_t *alt_state;
    size_t alt_id;
    _Atomic bool ref;
} channel_waiter_hdr_;

/* Buffered waiters currently only use the fields in the shared header. */
typedef struct channel_waiter_hdr_ channel_waiter_buf_;

typedef struct channel_waiter_unbuf_ {
    struct channel_waiter_unbuf_ *next, *prev;
    ch_sem_ *sem;
    _Atomic size_t *alt_state;
    size_t alt_id;
    _Atomic bool ref;
    bool closed;
    void *msg;
} channel_waiter_unbuf_;

typedef union channel_waiter_ {
    channel_waiter_root_ root;
    channel_waiter_hdr_ hdr;
    channel_waiter_buf_ buf;
    channel_waiter_unbuf_ unbuf;
} channel_waiter_;

typedef struct channel_hdr_ {
    uint32_t cap, msgsize;
    _Atomic uint32_t openc, refc;
    channel_waiter_root_ sendq, recvq;
    ch_mutex_ lock;
} channel_hdr_;

/* If C had generics, the cell struct would be defined as follows, except it
 * isn't ever alignment padded (TODO? might be a problem on some archs):
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
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        _Atomic uint32_t idx, lap;
#else
        _Atomic uint32_t lap, idx;
#endif
    };
} channel_aun64_;

typedef union channel_un64_ {
    uint64_t u64;
    struct {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        uint32_t idx, lap;
#else
        uint32_t lap, idx;
#endif
    };
} channel_un64_;

typedef struct channel_buf_ {
    uint32_t cap, msgsize;
    _Atomic uint32_t openc, refc;
    channel_waiter_root_ sendq, recvq;
    ch_mutex_ lock;
    channel_aun64_ write;
    char pad[64 - sizeof(channel_aun64_)]; // Cache line padding
    channel_aun64_ read;
    char pad1[64 - sizeof(channel_aun64_)];
    char buf[]; // channel_cell_<T> buf[];
} channel_buf_;

/* Unbuffered channels currently only use the fields in the shared header. */
typedef struct channel_hdr_ channel_unbuf_;

union channel {
    channel_hdr_ hdr;
    channel_buf_ buf;
    channel_unbuf_ unbuf;
};

struct channel_case {
    channel *c;
    void *msg;
    channel_op op;
    channel_waiter_ _w;
};

typedef enum channel_alt_rc_ {
    CH_ALT_READY_,
    CH_ALT_WAIT_,
    CH_ALT_CLOSED_,
} channel_alt_rc_;

#define CH_ALT_NIL_ CH_WBLOCK
#define CH_ALT_MAGIC_ CH_CLOSED

#define ch_load_rlx_(obj) atomic_load_explicit(obj, memory_order_relaxed)
#define ch_load_acq_(obj) atomic_load_explicit(obj, memory_order_acquire)
#define ch_load_seq_(obj) atomic_load_explicit(obj, memory_order_seq_cst)
#define ch_store_rlx_(obj, des) \
    atomic_store_explicit(obj, des, memory_order_relaxed)
#define ch_store_rel_(obj, des) \
    atomic_store_explicit(obj, des, memory_order_release)
#define ch_store_seq_(obj, des) \
    atomic_store_explicit(obj, des, memory_order_seq_cst)
#define ch_faa_rlx_(obj, arg) \
    atomic_fetch_add_explicit(obj, arg, memory_order_relaxed)
#define ch_fas_acr_(obj, arg) \
    atomic_fetch_sub_explicit(obj, arg, memory_order_acq_rel)
#define ch_cas_w_seq_acq_(obj, exp, des) \
    atomic_compare_exchange_weak_explicit( \
        obj, exp, des, memory_order_seq_cst, memory_order_acquire)
#define ch_cas_s_acr_rlx_(obj, exp, des) \
    atomic_compare_exchange_strong_explicit( \
        obj, exp, des, memory_order_acq_rel, memory_order_relaxed)

/* `ch_assert_` never becomes a noop, even when `NDEBUG` is set. */
#define ch_assert_(pred) \
    (__builtin_expect(!(pred), 0) ? \
        channel_assert_(__FILE__, __LINE__, #pred) : (void)0)

__attribute__((noreturn)) inline void
channel_assert_(const char *file, unsigned line, const char *pred) {
    fprintf(stderr, "Failed assertion: %s, %u, %s\n", file, line, pred);
    abort();
}

inline channel *
channel_make(size_t msgsize, size_t cap) {
    channel *c;
    if (cap == 0) {
        ch_assert_(msgsize <= UINT32_MAX && (c = calloc(1, sizeof(c->unbuf))));
    } else {
        ch_assert_(cap <= UINT32_MAX && cap <= SIZE_MAX / msgsize);
        ch_assert_((c = calloc(
            1, offsetof(channel_buf_, buf) + (cap * ch_cellsize_(msgsize)))));
        c->hdr.cap = cap;
        ch_store_rlx_(&c->buf.read.lap, 1);
    }
    c->hdr.msgsize = msgsize;
    ch_store_rlx_(&c->hdr.openc, 1);
    ch_store_rlx_(&c->hdr.refc, 1);
    ch_store_rlx_(&c->hdr.sendq.next, (channel_waiter_ *)&c->hdr.sendq);
    ch_store_rlx_(&c->hdr.sendq.prev, (channel_waiter_ *)&c->hdr.sendq);
    ch_store_rlx_(&c->hdr.recvq.next, (channel_waiter_ *)&c->hdr.recvq);
    ch_store_rlx_(&c->hdr.recvq.prev, (channel_waiter_ *)&c->hdr.recvq);
    ch_mutex_init_(c->hdr.lock);
    return c;
}

inline channel *
channel_dup(channel *c) {
    uint32_t prev = ch_faa_rlx_(&c->hdr.refc, 1);
    ch_assert_(0 < prev && prev < UINT32_MAX);
    return c;
}

inline channel *
channel_drop(channel *c) {
    switch (ch_fas_acr_(&c->hdr.refc, 1)) {
    case 0: ch_assert_(false);
    case 1:
        ch_mutex_lock_(&c->hdr.lock);
        ch_mutex_unlock_(&c->hdr.lock);
        ch_assert_(ch_mutex_destroy_(&c->hdr.lock) == 0);
        free(c); // fallthrough
    default: return NULL;
    }
}

inline void
channel_waitq_push_(channel_waiter_root_ *waitq, channel_waiter_ *w) {
    w->hdr.next = (channel_waiter_ *)waitq;
    w->hdr.prev = ch_load_rlx_(&waitq->prev);
    ch_store_seq_(&ch_load_rlx_(&waitq->prev)->root.next, w);
    ch_store_rlx_(&waitq->prev, w);
}

inline channel_waiter_ *
channel_waitq_shift_(channel_waiter_root_ *waitq) {
    channel_waiter_ *w = ch_load_rlx_(&waitq->next);
    if (&w->root == waitq) {
        return NULL;
    }
    ch_store_seq_(&w->hdr.prev->root.next, w->hdr.next);
    ch_store_rlx_(&w->hdr.next->root.prev, w->hdr.prev);
    w->hdr.prev = NULL; // For channel_waitq_remove
    ch_store_rlx_(&w->hdr.ref, true);
    return w;
}

inline bool
channel_waitq_remove_(channel_waiter_ *w) {
    if (!w->hdr.prev) { // Already shifted off the front
        return false;
    }
    ch_store_seq_(&w->hdr.prev->root.next, w->hdr.next);
    ch_store_rlx_(&w->hdr.next->root.prev, w->hdr.prev);
    return true;
}

inline channel *
channel_open(channel *c) {
    uint32_t prev = ch_faa_rlx_(&c->hdr.openc, 1);
    ch_assert_(0 < prev && prev < UINT32_MAX);
    return c;
}

inline channel *
channel_close(channel *c) {
    switch (ch_fas_acr_(&c->hdr.openc, 1)) {
    case 0: ch_assert_(false);
    case 1:
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
        ch_mutex_unlock_(&c->hdr.lock); // fallthrough
    default: return NULL;
    }
}

inline void
channel_buf_waitq_shift_(channel_waiter_root_ *waitq, ch_mutex_ *lock) {
    while (&ch_load_seq_(&waitq->next)->root != waitq) {
        ch_mutex_lock_(lock);
        channel_waiter_buf_ *w = &channel_waitq_shift_(waitq)->buf;
        ch_mutex_unlock_(lock);
        if (w) {
            if (w->alt_state) {
                size_t magic = CH_ALT_MAGIC_;
                if (!ch_cas_s_acr_rlx_(w->alt_state, &magic, w->alt_id)) {
                    ch_store_rel_(&w->ref, false);
                    continue;
                }
            }
            ch_sem_post_(w->sem);
        }
        return;
    }
}

inline channel_rc
channel_buf_trysend_(channel_buf_ *c, void *msg) {
    if (ch_load_acq_(&c->openc) > 0) {
        int i = 0;
        channel_un64_ write = {ch_load_acq_(&c->write.u64)};
        do {
            char *cell = c->buf + (write.idx * ch_cellsize_(c->msgsize));
            uint32_t lap = ch_load_acq_(ch_cell_lap_(cell));
            if (write.lap == lap) {
                uint64_t write1 = write.idx + 1 < c->cap ?
                    write.u64 + 1 : (uint64_t)(write.lap + 2) << 32;
                if (!ch_cas_w_seq_acq_(&c->write.u64, &write.u64, write1)) {
                    continue;
                }
                memcpy(ch_cell_msg_(cell), msg, c->msgsize);
                ch_store_rel_(ch_cell_lap_(cell), lap + 1);
                channel_buf_waitq_shift_(&c->recvq, &c->lock);
                return CH_OK;
            }

            if (write.lap > lap) {
                if (++i > 4) {
                    return CH_WBLOCK;
                }
                sched_yield();
            }
            write.u64 = ch_load_acq_(&c->write.u64);
        } while (ch_load_acq_(&c->openc) > 0);
    }
    return CH_CLOSED;
}

inline channel_rc
channel_buf_tryrecv_(channel_buf_ *c, void *msg) {
    channel_un64_ read = {ch_load_acq_(&c->read.u64)};
    for (int i = 0; ; ) {
        char *cell = c->buf + (read.idx * ch_cellsize_(c->msgsize));
        uint32_t lap = ch_load_acq_(ch_cell_lap_(cell));
        if (read.lap == lap) {
            uint64_t read1 = read.idx + 1 < c->cap ?
                read.u64 + 1 : (uint64_t)(read.lap + 2) << 32;
            if (!ch_cas_w_seq_acq_(&c->read.u64, &read.u64, read1)) {
                continue;
            }
            memcpy(msg, ch_cell_msg_(cell), c->msgsize);
            ch_store_rel_(ch_cell_lap_(cell), lap + 1);
            channel_buf_waitq_shift_(&c->sendq, &c->lock);
            return CH_OK;
        }

        if (read.lap > lap) {
            if (ch_load_acq_(&c->openc) == 0) {
                return CH_CLOSED;
            }
            if (++i > 4) {
                return CH_WBLOCK;
            }
            sched_yield();
        }
        read.u64 = ch_load_acq_(&c->read.u64);
    }
}

inline channel_rc
channel_unbuf_try_(channel_unbuf_ *c, void *msg, channel_waiter_root_ *waitq) {
    while (ch_load_acq_(&c->openc) > 0) {
        if (&ch_load_acq_(&waitq->next)->root == waitq) {
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
        if (w->alt_state) {
            size_t magic = CH_ALT_MAGIC_;
            if (!ch_cas_s_acr_rlx_(w->alt_state, &magic, w->alt_id)) {
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
    return CH_CLOSED;
}

inline channel_rc
channel_buf_send_or_wait_(channel_buf_ *c, void *msg, channel_waiter_buf_ *w) {
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
        char *cell = c->buf + (write.idx * ch_cellsize_(c->msgsize));
        if (write.lap == ch_load_acq_(ch_cell_lap_(cell))) {
            channel_waitq_remove_((channel_waiter_ *)w);
            ch_mutex_unlock_(&c->lock);
            continue;
        }
        ch_mutex_unlock_(&c->lock);
        return CH_WBLOCK;
    }
}

inline channel_rc
channel_buf_recv_or_wait_(channel_buf_ *c, void *msg, channel_waiter_buf_ *w) {
    for ( ; ; ) {
        channel_rc rc = channel_buf_tryrecv_(c, msg);
        if (rc != CH_WBLOCK) {
            return rc;
        }

        ch_mutex_lock_(&c->lock);
        channel_waitq_push_(&c->recvq, (channel_waiter_ *)w);
        channel_un64_ read = {ch_load_acq_(&c->read.u64)};
        char *cell = c->buf + (read.idx * ch_cellsize_(c->msgsize));
        if (read.lap == ch_load_acq_(ch_cell_lap_(cell))) {
            channel_waitq_remove_((channel_waiter_ *)w);
            ch_mutex_unlock_(&c->lock);
            continue;
        }
        if (ch_load_acq_(&c->openc) == 0) {
            channel_waitq_remove_((channel_waiter_ *)w);
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
    channel_waiter_buf_ w = {.sem = &sem, .alt_id = CH_ALT_NIL_};

    for ( ; ; ) {
        channel_rc rc = channel_buf_send_or_wait_(c, msg, &w);
        if (rc != CH_WBLOCK) {
            ch_sem_destroy_(&sem);
            return rc;
        }

        if (timeout == NULL) {
            ch_sem_wait_(&sem);
        } else if (ch_sem_timedwait_(&sem, timeout) != 0) { // != 0 due to OS X
            ch_mutex_lock_(&c->lock);
            bool onqueue =
                channel_waitq_remove_((channel_waiter_ *)&w);
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
    channel_waiter_buf_ w = {.sem = &sem, .alt_id = CH_ALT_NIL_};

    for ( ; ; ) {
        channel_rc rc = channel_buf_recv_or_wait_(c, msg, &w);
        if (rc != CH_WBLOCK) {
            ch_sem_destroy_(&sem);
            return rc;
        }

        if (timeout == NULL) {
            ch_sem_wait_(&sem);
        } else if (ch_sem_timedwait_(&sem, timeout) != 0) {
            ch_mutex_lock_(&c->lock);
            bool onqueue =
                channel_waitq_remove_((channel_waiter_ *)&w);
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
channel_unbuf_rendez_or_wait_(
    channel_unbuf_ *c, void *msg, channel_waiter_unbuf_ *w, channel_op op
) {
    channel_waiter_root_ *shiftq, *pushq;
    if (op == CH_SEND) {
        shiftq = &c->recvq;
        pushq = &c->sendq;
    } else {
        shiftq = &c->sendq;
        pushq = &c->recvq;
    }

    while (ch_load_acq_(&c->openc) > 0) {
        ch_mutex_lock_(&c->lock);
        if (ch_load_acq_(&c->openc) == 0) {
            ch_mutex_unlock_(&c->lock);
            return CH_CLOSED;
        }
        channel_waiter_unbuf_ *w1 = &channel_waitq_shift_(shiftq)->unbuf;
        if (w1) {
            ch_mutex_unlock_(&c->lock);
            if (w1->alt_state) {
                size_t magic = CH_ALT_MAGIC_;
                if (!ch_cas_s_acr_rlx_(w1->alt_state, &magic, w1->alt_id)) {
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
    return CH_CLOSED;
}

inline channel_rc
channel_unbuf_rendez_(
    channel_unbuf_ *c, void *msg, ch_timespec_ *timeout, channel_op op
) {
    ch_sem_ sem;
    ch_sem_init_(&sem, 0, 0);
    channel_waiter_unbuf_ w = {.sem = &sem, .alt_id = CH_ALT_NIL_, .msg = msg};
    channel_rc rc = channel_unbuf_rendez_or_wait_(c, msg, &w, op);
    if (rc != CH_WBLOCK) {
        ch_sem_destroy_(&sem);
        return rc;
    }

    if (timeout == NULL) {
        ch_sem_wait_(&sem);
    } else if (ch_sem_timedwait_(&sem, timeout) != 0) {
        ch_mutex_lock_(&c->lock);
        bool onqueue = channel_waitq_remove_((channel_waiter_ *)&w);
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

inline channel_rc
channel_send(channel *c, void *msg, size_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    return c->hdr.cap > 0 ?
        channel_buf_send_(&c->buf, msg, NULL) :
        channel_unbuf_rendez_(&c->unbuf, msg, NULL, CH_SEND);
}

inline channel_rc
channel_recv(channel *c, void *msg, size_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    return c->hdr.cap > 0 ?
        channel_buf_recv_(&c->buf, msg, NULL) :
        channel_unbuf_rendez_(&c->unbuf, msg, NULL, CH_RECV);
}

inline channel_rc
channel_trysend(channel *c, void *msg, size_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    return c->hdr.cap > 0 ?
        channel_buf_trysend_(&c->buf, msg) :
        channel_unbuf_try_(&c->unbuf, msg, &c->unbuf.recvq);
}

inline channel_rc
channel_tryrecv(channel *c, void *msg, size_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    return c->hdr.cap > 0 ?
        channel_buf_tryrecv_(&c->buf, msg) :
        channel_unbuf_try_(&c->unbuf, msg, &c->unbuf.sendq);
}

inline channel_rc
channel_timedsend(channel *c, void *msg, uint64_t timeout, size_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    ch_timespec_ ts = channel_add_timeout_(timeout);
    return c->hdr.cap > 0 ?
        channel_buf_send_(&c->buf, msg, &ts) :
        channel_unbuf_rendez_(&c->unbuf, msg, &ts, CH_SEND);
}

inline channel_rc
channel_timedrecv(channel *c, void *msg, uint64_t timeout, size_t msgsize) {
    ch_assert_(msgsize == c->hdr.msgsize);
    ch_timespec_ ts = channel_add_timeout_(timeout);
    return c->hdr.cap > 0 ?
        channel_buf_recv_(&c->buf, msg, &ts) :
        channel_unbuf_rendez_(&c->unbuf, msg, &ts, CH_RECV);
}

inline size_t
channel_tryalt(channel_case cases[], size_t len, size_t offset) {
    size_t closedc = 0;
    for (size_t i = 0; i < len; i++) {
        channel_case *cc = cases + ((i + offset) % len);
        if (cc->op == CH_NOOP) {
            closedc++;
            continue;
        }

        switch (
            cc->op == CH_SEND ?
                channel_trysend(cc->c, cc->msg, cc->c->hdr.msgsize) :
                channel_tryrecv(cc->c, cc->msg, cc->c->hdr.msgsize)
        ) {
        case CH_OK: return (i + offset) % len;
        case CH_WBLOCK: break;
        case CH_CLOSED: closedc++;
        }
    }
    return closedc == len ? CH_CLOSED : CH_WBLOCK;
}

inline bool
channel_alt_ready_(channel *c, channel_op op) {
    if (c->hdr.cap == 0) {
        return op == CH_SEND ?
            &ch_load_acq_(&c->unbuf.recvq.next)->root != &c->unbuf.recvq :
            &ch_load_acq_(&c->unbuf.sendq.next)->root != &c->unbuf.sendq;
    }
    channel_un64_ u = op == CH_SEND ?
        (const channel_un64_){ch_load_acq_(&c->buf.write.u64)} :
        (const channel_un64_){ch_load_acq_(&c->buf.read.u64)};
    char *cell = c->buf.buf + (u.idx * ch_cellsize_(c->buf.msgsize));
    return u.lap <= ch_load_acq_(ch_cell_lap_(cell));
}

inline channel_alt_rc_
channel_alt_wait_(
    channel_case cases[static 1],
    size_t len,
    size_t offset,
    ch_sem_ *sem,
    _Atomic size_t *state
) {
    size_t closedc = 0;
    for (size_t i = 0; i < len; i++) {
        channel_case *cc = cases + ((i + offset) % len);
        if (cc->op == CH_NOOP || ch_load_acq_(&cc->c->hdr.openc) == 0) {
            closedc++;
            continue;
        }

        if (cc->c->hdr.cap == 0) {
            cc->_w.unbuf.msg = cc->msg;
        }
        cc->_w.hdr.sem = sem;
        cc->_w.hdr.alt_state = state;
        cc->_w.hdr.alt_id = (i + offset) % len;
        channel_waiter_root_ *waitq = cc->op == CH_SEND ?
            &cc->c->hdr.sendq : &cc->c->hdr.recvq;
        ch_mutex_lock_(&cc->c->hdr.lock);
        channel_waitq_push_(waitq, &cc->_w);
        if (channel_alt_ready_(cc->c, cc->op)) {
            channel_waitq_remove_(&cc->_w);
            ch_mutex_unlock_(&cc->c->hdr.lock);
            cc->_w.hdr.sem = NULL;
            return CH_ALT_READY_;
        }
        ch_mutex_unlock_(&cc->c->hdr.lock);
    }

    if (closedc == len) {
        return CH_ALT_CLOSED_;
    }
    return CH_ALT_WAIT_;
}

inline void
channel_alt_remove_waiters_(
    channel_case cases[static 1], size_t len, size_t state
) {
    for (size_t i = 0; i < len; i++) {
        channel_case *cc = cases + i;
        if (
            cc->op == CH_NOOP ||
            ch_load_acq_(&cc->c->hdr.openc) == 0 ||
            !cc->_w.hdr.sem
        ) {
            continue;
        }

        ch_mutex_lock_(&cc->c->hdr.lock);
        bool onqueue = channel_waitq_remove_(&cc->_w);
        ch_mutex_unlock_(&cc->c->hdr.lock);
        if (!onqueue && i != state) {
            while (ch_load_acq_(&cc->_w.hdr.ref)) {
                sched_yield();
            }
        }
        cc->_w.hdr.sem = NULL;
    }
}

inline size_t
channel_alt(channel_case cases[], size_t len, uint64_t timeout) {
    ch_timespec_ ts;
    if (timeout < UINT64_MAX) {
        ts = channel_add_timeout_(timeout);
    }
    size_t offset = rand();
    ch_sem_ sem;
    ch_sem_init_(&sem, 0, 0);
    bool timedout = false;
    do {
        size_t idx = channel_tryalt(cases, len, offset);
        if (idx != CH_WBLOCK) {
            return idx;
        }

        _Atomic size_t state;
        ch_store_rlx_(&state, CH_ALT_MAGIC_);
        size_t state1 = CH_ALT_MAGIC_;
        switch (channel_alt_wait_(cases, len, offset, &sem, &state)) {
        case CH_ALT_CLOSED_: return CH_CLOSED;
        case CH_ALT_READY_:
            if (!ch_cas_s_acr_rlx_(&state, &state1, CH_ALT_NIL_)) {
                ch_sem_wait_(&sem);
            }
            break;
        case CH_ALT_WAIT_:
            if (timeout == UINT64_MAX) {
                ch_sem_wait_(&sem);
            } else if (ch_sem_timedwait_(&sem, &ts) != 0) {
                timedout = true;
                if (!ch_cas_s_acr_rlx_(&state, &state1, CH_ALT_NIL_)) {
                    ch_sem_wait_(&sem);
                }
                break;
            }
            ch_cas_s_acr_rlx_(&state, &state1, CH_ALT_NIL_);
        }

        channel_alt_remove_waiters_(cases, len, state1);
        if (state1 != CH_ALT_MAGIC_) {
            channel_case *cc = cases + state1;
            if (
                cc->c->hdr.cap == 0 ||
                (cc->op == CH_SEND && channel_buf_trysend_(
                    &cc->c->buf, cc->msg) == CH_OK) ||
                (cc->op == CH_RECV && channel_buf_tryrecv_(
                    &cc->c->buf, cc->msg) == CH_OK)
            ) {
                return state1;
            }
        }
    } while (!timedout);
    return CH_WBLOCK;
}
#endif
