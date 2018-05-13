/* Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file.
 *
 * The semantics of this library was inspired by the Go programming language.
 *
 * The implementations of sending, receiving, and selection were inspired by
 * algorithms described in "Go channels on steroids" by Dmitry Vyukov. */
#ifndef CHANNEL_H
#define CHANNEL_H
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if _POSIX_SEMAPHORES >= 200112L
#include <semaphore.h>
#elif defined __APPLE__
#include <dispatch/dispatch.h>
#else
#error "insert semaphore implementation here"
#endif

#if ATOMIC_LLONG_LOCK_FREE != 2
#error "not sure if this works if _Atomic uint64_t isn't lock-free"
#elif !defined(__BYTE_ORDER__) || (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error "needs trivial changes for big endian but i don't have a box to test on"
#endif

#define load_rlx_(obj) atomic_load_explicit(obj, memory_order_relaxed)
#define load_acq_(obj) atomic_load_explicit(obj, memory_order_acquire)
#define store_rlx_(obj, des) \
    atomic_store_explicit(obj, des, memory_order_relaxed)
#define store_rel_(obj, des) \
    atomic_store_explicit(obj, des, memory_order_release)
#define faa_acq_rel_(obj, arg) \
    atomic_fetch_add_explicit(obj, arg, memory_order_acq_rel)
#define fas_acq_rel_(obj, arg) \
    atomic_fetch_sub_explicit(obj, arg, memory_order_acq_rel)
#define cas_weak_(obj, exp, des) \
    atomic_compare_exchange_weak_explicit( \
        obj, \
        exp, \
        des, \
        memory_order_acq_rel, \
        memory_order_acquire)

#if _POSIX_SEMAPHORES < 200112L && defined __APPLE__
#define sem_t dispatch_semaphore_t
#define sem_init(sem, pshared, val) *(sem) = dispatch_semaphore_create(val)
#define sem_post(sem) dispatch_semaphore_signal(*(sem))
#define sem_wait(sem) dispatch_semaphore_wait(*(sem), DISPATCH_TIME_FOREVER)
#define sem_destroy(sem) dispatch_release(*(sem))
#endif

/* Exported "functions" */
#define ch_make(type, cap) channel_make(sizeof(uint32_t) + sizeof(type), cap)
#define ch_dup(c) channel_dup(c)
#define ch_close(c) channel_close(c)
#define ch_drop(c) channel_drop(c)

#define ch_send(c, type, elt) channel_send(c, sizeof(type), &elt)
#define ch_trysend(c, type, elt) channel_trysend(c, sizeof(type), &elt)
#define ch_forcesend(c, type, elt) channel_forcesend(c, sizeof(type), &elt)

#define ch_recv(c, type, elt) channel_recv(c, sizeof(type), &elt)
#define ch_tryrecv(c, type, elt) channel_tryrecv(c, sizeof(type), &elt)

/* XXX TODO */
#if 0
#define ch_timedsend(c, type, offset, elt) \
    channel_timedsend(c, sizeof(type), offset, &elt)

#define ch_timedrecv(c, type, offset, elt) \
    channel_timedrecv(c, sizeof(type), offset, &elt)

#define ch_sel_init(cap) channel_select_init(cap)
#define ch_sel_finish(sel) channel_select_finish(sel)
#define ch_sel_reg(sel, c, op) channel_select_reg(sel, c, op)
#define ch_select(sel) channel_select_do(sel)
#define ch_sel_send(c, type, elt) channel_select_send(c, sizeof(type), &elt)
#define ch_sel_recv(c, type, elt) channel_select_recv(c, sizeof(type), &elt)
#endif

/* Return codes */
#define CH_OK 0x0
#define CH_CLOSED 0x1
#define CH_FULL 0x2
#define CH_EMPTY 0x4
#define CH_WBLOCK 0x8
#define CH_TIMEDOUT 0x10

/* Polling alternative to ch_select. Continually loops over the cases (`op` is
 * intended to be either ch_trysend or ch_tryrecv) rather than blocking, which
 * may be preferable in cases where one of the operations is expected to
 * succeed or if there is a default case. Burns a lot of cycles otherwise. */
#define ch_poll(casec) if (1) { \
    bool Xdone_ = false; \
    switch (rand() % casec) { \
    for ( ; ; Xdone_ = true)

#define ch_case(id, op, ...) \
    case id: \
        if (op == CH_OK) { \
            __VA_ARGS__; \
            break; \
        } else (void)0

#define ch_default(...) \
    if (Xdone_) { \
        __VA_ARGS__; \
        break; \
    } else (void)0

#define ch_poll_end } } else (void)0

/* These declarations must be present in exactly one compilation unit */
#define CHANNEL_EXTERN_DECL \
    extern inline channel *channel_make(size_t, uint32_t); \
    extern inline channel *channel_dup(channel *); \
    extern inline channel *channel_drop(channel *); \
    extern inline channel_waiter_ *channel_waitq_add_( \
        channel_waiter_ **, void *); \
    extern inline bool channel_waitq_rm_( \
        channel_waiter_ **, channel_waiter_ *); \
    extern inline channel_waiter_ *channel_waitq_shift_(channel_waiter_ **); \
    extern inline void channel_waiter_drop_(channel_waiter_ *); \
    extern inline void channel_close(channel *); \
    extern inline bool channel_signal_select_(channel *, uint8_t); \
    extern inline int channel_trysend_buf_(channel_buf_ *, size_t, void *); \
    extern inline int channel_trysend_unbuf_( \
        channel_unbuf_ *, size_t, void *); \
    extern inline int channel_trysend(channel *, size_t, void *); \
    extern inline int channel_tryrecv_buf_(channel_buf_ *, size_t, void *); \
    extern inline int channel_tryrecv_unbuf_( \
        channel_unbuf_ *, size_t, void *); \
    extern inline int channel_tryrecv(channel *, size_t, void *); \
    extern inline int channel_send_buf_(channel_buf_ *, size_t, void *); \
    extern inline int channel_send_unbuf_(channel_unbuf_ *, size_t, void *); \
    extern inline int channel_send(channel *, size_t, void *); \
    extern inline int channel_recv_buf_(channel_buf_ *, size_t, void *); \
    extern inline int channel_recv_unbuf_(channel_unbuf_ *, size_t, void *); \
    extern inline int channel_recv(channel *, size_t, void *); \
    extern inline int channel_forcesend(channel *, size_t, void *)

typedef union aun64_ {
    _Atomic uint64_t u64;
    struct {
        _Atomic uint32_t index;
        _Atomic uint32_t lap;
    } u32;
} aun64_;

typedef union un64_ {
    uint64_t u64;
    struct {
        uint32_t index;
        uint32_t lap;
    } u32;
} un64_;

typedef struct channel_waiter_hdr_ {
    union channel_waiter_ *prev, *next;
    sem_t sem;
} channel_waiter_hdr_;

/* Buffered waiters currently only use the fields in the shared header */
typedef struct channel_waiter_hdr_ channel_waiter_buf_;

typedef struct channel_waiter_unbuf_ {
    struct channel_waiter_unbuf_ *prev, *next;
    sem_t sem;
    void *elt;
    bool closed;
} channel_waiter_unbuf_;

typedef union channel_waiter_ {
    channel_waiter_hdr_ hdr;
    channel_waiter_buf_ buf;
    channel_waiter_unbuf_ unbuf;
} channel_waiter_;

typedef struct channel_hdr_ {
    uint32_t cap;
    _Atomic uint32_t refc;
    channel_waiter_ *sendq, *recvq;
    pthread_mutex_t lock;
} channel_hdr_;

typedef struct channel_buf_ {
    uint32_t cap;
    _Atomic uint32_t refc;
    channel_waiter_ *sendq, *recvq;
    pthread_mutex_t lock;
    aun64_ write, read;
    char buf[];
} channel_buf_;

/* Unbuffered channels currently only use the fields in the shared header. */
typedef struct channel_hdr_ channel_unbuf_;

typedef union channel {
    channel_hdr_ hdr;
    channel_buf_ buf;
    channel_unbuf_ unbuf;
} channel;

/* If the capacity is 0 then the channel is unbuffered. Otherwise the channel
 * is buffered. */
inline channel *
channel_make(size_t boxsize, uint32_t cap) {
    channel *c;
    if (cap > 0) {
        assert((c = calloc(1, offsetof(channel_buf_, buf) + (cap * boxsize))));
        store_rlx_(&c->buf.read.u32.lap, 1);
    } else {
        assert((c = calloc(1, sizeof(c->unbuf))));
    }
    c->hdr.cap = cap;
    store_rlx_(&c->hdr.refc, 1);
    c->hdr.lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    return c;
}

/* Increases the reference count of the channel and returns it. Intended to be
 * used with multiple producers that independently close the channel. */
inline channel *
channel_dup(channel *c) {
    assert(faa_acq_rel_(&c->hdr.refc, 1) != 0);
    return c;
}

/* Deallocates all resources associated with the channel. */
inline channel *
channel_drop(channel *c) {
    assert(pthread_mutex_destroy(&c->hdr.lock) == 0);
    free(c);
    return NULL;
}

inline channel_waiter_ *
channel_waitq_add_(channel_waiter_ **waitq, void *elt) {
    channel_waiter_ *w;
    if (!elt) {
        assert((w = malloc(sizeof(w->buf))));
    } else {
        assert((w = malloc(sizeof(w->unbuf))));
        w->unbuf.elt = elt;
        w->unbuf.closed = false;
    }
    w->hdr.next = NULL;
    channel_waiter_ *wq = *waitq;
    if (!wq) {
        *waitq = w;
        w->hdr.prev = w;
    } else {
        w->hdr.prev = wq->hdr.prev;
        wq->hdr.prev = w;
        w->hdr.prev->hdr.next = w;
    }
    sem_init(&w->hdr.sem, 0, 0);
    return w;
}

inline bool
channel_waitq_rm_(channel_waiter_ **waitq, channel_waiter_ *w) {
    if (!w->hdr.prev) { // Already shifted off the front
        return false;
    }
    if (w->hdr.prev == w) { // `w` is the only element
        *waitq = NULL;
        return true;
    }

    if (w->hdr.prev->hdr.next) {
        w->hdr.prev->hdr.next = w->hdr.next;
    }
    if (w->hdr.next) {
        w->hdr.next->hdr.prev = w->hdr.prev;
    } else {
        (*waitq)->hdr.prev = w->hdr.prev;
    }
    return true;
}

inline channel_waiter_ *
channel_waitq_shift_(channel_waiter_ **waitq) {
    channel_waiter_ *w = *waitq;
    if (w) {
        if (w->hdr.next) {
            w->hdr.next->hdr.prev = w->hdr.prev;
        }
        *waitq = w->hdr.next;
        w->hdr.prev = NULL; // For channel_waitq_rm_
    }
    return w;
}

inline void
channel_waiter_drop_(channel_waiter_ *w) {
    sem_destroy(&w->hdr.sem);
    free(w);
}

/* Closes the channel if the caller has the last reference to the channel.
 * Decreases the reference count otherwise. Attempting to send on a closed
 * channel immediately returns an error code. Receiving on a closed channel
 * succeeds until the channel is emptied. */
inline void
channel_close(channel *c) {
    uint32_t refc = fas_acq_rel_(&c->hdr.refc, 1);
    if (refc != 1) {
        assert(refc != 0);
        return;
    }

    pthread_mutex_lock(&c->hdr.lock);
    if (c->hdr.cap > 0) {
        while (c->buf.sendq) {
            channel_waiter_ *w = channel_waitq_shift_(&c->buf.sendq);
            sem_post(&w->buf.sem);
        }
        while (c->buf.recvq) {
            channel_waiter_ *w = channel_waitq_shift_(&c->buf.recvq);
            sem_post(&w->buf.sem);
        }
    } else {
        while (c->unbuf.sendq) {
            channel_waiter_ *w = channel_waitq_shift_(&c->unbuf.sendq);
            w->unbuf.closed = true;
            sem_post(&w->unbuf.sem);
        }
        while (c->unbuf.recvq) {
            channel_waiter_ *w = channel_waitq_shift_(&c->unbuf.recvq);
            w->unbuf.closed = true;
            sem_post(&w->unbuf.sem);
        }
    }
    pthread_mutex_unlock(&c->hdr.lock);
}

/* Nonblocking sends on buffered channels fail if the buffer is full. */
inline int
channel_trysend_buf_(channel_buf_ *c, size_t eltsize, void *elt) {
    for (int i = 0; ; ) {
        if (load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }
        un64_ write = {load_acq_(&c->write.u64)};
        char *box = c->buf + (write.u32.index * (eltsize + sizeof(uint32_t)));
        uint32_t lap = load_acq_((_Atomic uint32_t *)box);
        if (write.u32.lap != lap) {
            if (i++ < 4) { // Improves benchmarks; harmful in normal usage?
                sched_yield();
                continue;
            }
            return CH_FULL;
        }

        uint64_t write1 = write.u32.index + 1 < c->cap ?
                write.u64 + 1 : (uint64_t)(write.u32.lap + 2) << 32;
        if (cas_weak_(&c->write.u64, &write.u64, write1)) {
            memcpy(box + sizeof(uint32_t), elt, eltsize);
            store_rel_((_Atomic uint32_t *)box, lap + 1);
            if (c->recvq) {
                pthread_mutex_lock(&c->lock);
                channel_waiter_ *w = channel_waitq_shift_(&c->recvq);
                pthread_mutex_unlock(&c->lock);
                if (w) {
                    sem_post(&w->buf.sem);
                }
            }
            return CH_OK;
        }
    }
}

/* Nonblocking sends on unbuffered channels fail if there is no receiver. */
inline int
channel_trysend_unbuf_(channel_unbuf_ *c, size_t eltsize, void *elt) {
    if (load_acq_(&c->refc) == 0) {
        return CH_CLOSED;
    }
    if (!c->recvq) {
        return CH_WBLOCK;
    }

    pthread_mutex_lock(&c->lock);
    if (load_acq_(&c->refc) == 0) {
        pthread_mutex_unlock(&c->lock);
        return CH_CLOSED;
    }
    channel_waiter_ *w = channel_waitq_shift_(&c->recvq);
    pthread_mutex_unlock(&c->lock);
    if (!w) {
        return CH_WBLOCK;
    }
    memcpy(w->unbuf.elt, elt, eltsize);
    sem_post(&w->unbuf.sem);
    return CH_OK;
}

/* Nonblocking send. See channel_trysend_buf_ and channel_trysend_unbuf_. */
inline int
channel_trysend(channel *c, size_t eltsize, void *elt) {
    if (c->hdr.cap > 0) {
        return channel_trysend_buf_(&c->buf, eltsize, elt);
    }
    return channel_trysend_unbuf_(&c->unbuf, eltsize, elt);
}

/* Nonblocking receives on buffered channels fail if the buffer is empty. */
inline int
channel_tryrecv_buf_(channel_buf_ *c, size_t eltsize, void *elt) {
    for (int i = 0; ; ) {
        un64_ read = {load_acq_(&c->read.u64)};
        char *box = c->buf + (read.u32.index * (eltsize + sizeof(uint32_t)));
        uint32_t lap = load_acq_((_Atomic uint32_t *)box);
        if (read.u32.lap != lap) {
            if (load_acq_(&c->refc) == 0) {
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
        if (cas_weak_(&c->read.u64, &read.u64, read1)) {
            memcpy(elt, box + sizeof(uint32_t), eltsize);
            store_rel_((_Atomic uint32_t *)box, lap + 1);
            if (c->sendq) {
                pthread_mutex_lock(&c->lock);
                channel_waiter_ *w = channel_waitq_shift_(&c->sendq);
                pthread_mutex_unlock(&c->lock);
                if (w) {
                    sem_post(&w->buf.sem);
                }
            }
            return CH_OK;
        }
    }
}

/* Nonblocking receives on unbuffered channels fail if there is no sender. */
inline int
channel_tryrecv_unbuf_(channel_unbuf_ *c, size_t eltsize, void *elt) {
    if (load_acq_(&c->refc) == 0) {
        return CH_CLOSED;
    }
    if (!c->sendq) {
        return CH_WBLOCK;
    }

    pthread_mutex_lock(&c->lock);
    if (load_acq_(&c->refc) == 0) {
        pthread_mutex_unlock(&c->lock);
        return CH_CLOSED;
    }
    channel_waiter_ *w = channel_waitq_shift_(&c->sendq);
    pthread_mutex_unlock(&c->lock);
    if (!w) {
        return CH_WBLOCK;
    }
    memcpy(elt, w->unbuf.elt, eltsize);
    sem_post(&w->unbuf.sem);
    return CH_OK;
}

/* Nonblocking receive. See channel_tryrecv_buf_ and channel_tryrecv_unbuf. */
inline int
channel_tryrecv(channel *c, size_t eltsize, void *elt) {
    if (c->hdr.cap > 0) {
        return channel_tryrecv_buf_(&c->buf, eltsize, elt);
    }
    return channel_tryrecv_unbuf_(&c->unbuf, eltsize, elt);
}

/* Blocking sends on buffered channels block while the buffer is full. */
inline int
channel_send_buf_(channel_buf_ *c, size_t eltsize, void *elt) {
    for ( ; ; ) {
        int rc = channel_trysend_buf_(c, eltsize, elt);
        if (rc == CH_OK || rc == CH_CLOSED) {
            return rc;
        }

        pthread_mutex_lock(&c->lock);
        if (load_acq_(&c->refc) == 0) {
            pthread_mutex_unlock(&c->lock);
            return CH_CLOSED;
        }
        channel_waiter_ *w = channel_waitq_add_(&c->sendq, NULL);
        un64_ write = {load_acq_(&c->write.u64)};
        char *elt = c->buf + (write.u32.index * (eltsize + sizeof(uint32_t)));
        if (write.u32.lap == load_acq_((_Atomic uint32_t *)elt)) {
            channel_waitq_rm_(&c->sendq, w);
            pthread_mutex_unlock(&c->lock);
            channel_waiter_drop_(w);
            continue;
        }
        pthread_mutex_unlock(&c->lock);
        sem_wait(&w->buf.sem);
        channel_waiter_drop_(w);
    }
}

/* Blocking sends on unbuffered channels block until there is a receiver. */
inline int
channel_send_unbuf_(channel_unbuf_ *c, size_t eltsize, void *elt) {
    if (load_acq_(&c->refc) == 0) {
        return CH_CLOSED;
    }

    pthread_mutex_lock(&c->lock);
    if (load_acq_(&c->refc) == 0) {
        pthread_mutex_unlock(&c->lock);
        return CH_CLOSED;
    }
    channel_waiter_ *w = channel_waitq_shift_(&c->recvq);
    if (w) {
        pthread_mutex_unlock(&c->lock);
        memcpy(w->unbuf.elt, elt, eltsize);
        sem_post(&w->unbuf.sem);
        return CH_OK;
    }

    channel_waiter_ *w1 = channel_waitq_add_(&c->sendq, elt);
    pthread_mutex_unlock(&c->lock);
    sem_wait(&w1->hdr.sem);
    int rc = w1->unbuf.closed ? CH_CLOSED : CH_OK;
    channel_waiter_drop_(w1);
    return rc;
}

/* Blocking send. See channel_send_buf_ and channel_send_unbuf_. */
inline int
channel_send(channel *c, size_t eltsize, void *elt) {
    if (c->hdr.cap > 0) {
        return channel_send_buf_(&c->buf, eltsize, elt);
    }
    return channel_send_unbuf_(&c->unbuf, eltsize, elt);
}

/* Blocking receives on buffered channels block when the buffer is empty. */
inline int
channel_recv_buf_(channel_buf_ *c, size_t eltsize, void *elt) {
    for ( ; ; ) {
        int rc = channel_tryrecv_buf_(c, eltsize, elt);
        if (rc == CH_OK || rc == CH_CLOSED) {
            return rc;
        }

        pthread_mutex_lock(&c->lock);
        channel_waiter_ *w = channel_waitq_add_(&c->recvq, NULL);
        un64_ read = {load_acq_(&c->read.u64)};
        char *elt = c->buf + (read.u32.index * (eltsize + sizeof(uint32_t)));
        if (read.u32.lap == load_acq_((_Atomic uint32_t *)elt)) {
            channel_waitq_rm_(&c->recvq, w);
            pthread_mutex_unlock(&c->lock);
            channel_waiter_drop_(w);
            continue;
        }
        if (load_acq_(&c->refc) == 0) {
            channel_waitq_rm_(&c->recvq, w);
            pthread_mutex_unlock(&c->lock);
            channel_waiter_drop_(w);
            return CH_CLOSED;
        }
        pthread_mutex_unlock(&c->lock);
        sem_wait(&w->buf.sem);
        channel_waiter_drop_(w);
    }
}

/* Blocking receives on unbuffered channels block when there is no sender. */
inline int
channel_recv_unbuf_(channel_unbuf_ *c, size_t eltsize, void *elt) {
    if (load_acq_(&c->refc) == 0) {
        return CH_CLOSED;
    }

    pthread_mutex_lock(&c->lock);
    if (load_acq_(&c->refc) == 0) {
        pthread_mutex_unlock(&c->lock);
        return CH_CLOSED;
    }
    channel_waiter_ *w = channel_waitq_shift_(&c->sendq);
    if (w) {
        pthread_mutex_unlock(&c->lock);
        memcpy(elt, w->unbuf.elt, eltsize);
        sem_post(&w->unbuf.sem);
        return CH_OK;
    }

    channel_waiter_ *w1 = channel_waitq_add_(&c->recvq, elt);
    pthread_mutex_unlock(&c->lock);
    sem_wait(&w1->unbuf.sem);
    int rc = w1->unbuf.closed ? CH_CLOSED : CH_OK;
    channel_waiter_drop_(w1);
    return rc;
}

/* Blocking receive. See channel_recv_buf_ and channel_recv_unbuf_. */
inline int
channel_recv(channel *c, size_t eltsize, void *elt) {
    if (c->hdr.cap > 0) {
        return channel_recv_buf_(&c->buf, eltsize, elt);
    }
    return channel_recv_unbuf_(&c->unbuf, eltsize, elt);
}

/* Forced sends on buffered channels do not block and overwrite the oldest
 * message if the buffer is full. Forced sends are not possible with unbuffered
 * channels. XXX This needs more testing. */
inline int
channel_forcesend(channel *c_, size_t eltsize, void *elt) {
    assert(c_->hdr.cap > 0);
    channel_buf_ *c = &c_->buf;
    for (bool full = false; ; ) {
        if (load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }
        un64_ write = {load_acq_(&c->write.u64)};
        char *box = c->buf + (write.u32.index * (eltsize + sizeof(uint32_t)));
        uint32_t lap = load_acq_((_Atomic uint32_t *)box);
        if (write.u32.lap != lap) {
            full = true;
            un64_ read = {load_acq_(&c->read.u64)};
            char *box1 =
                c->buf + (read.u32.index * (eltsize + sizeof(uint32_t)));
            uint32_t lap1 = load_acq_((_Atomic uint32_t *)box1);
            if (read.u32.lap != lap1) {
                continue;
            }

            uint64_t read1 = read.u32.index + 1 < c->cap ?
                    read.u64 + 1 : (uint64_t)(read.u32.lap + 2) << 32;
            if (cas_weak_(&c->read.u64, &read.u64, read1)) {
                store_rel_((_Atomic uint32_t *)box1, lap1 + 1);
            }
            continue;
        }

        uint64_t write1 = write.u32.index + 1 < c->cap ?
                write.u64 + 1 : (uint64_t)(write.u32.lap + 2) << 32;
        if (cas_weak_(&c->write.u64, &write.u64, write1)) {
            memcpy(box + sizeof(uint32_t), elt, eltsize);
            store_rel_((_Atomic uint32_t *)box, lap + 1);

            if (c->recvq) {
                pthread_mutex_lock(&c->lock);
                channel_waiter_ *w = channel_waitq_shift_(&c->recvq);
                pthread_mutex_unlock(&c->lock);
                if (w) {
                    sem_post(&w->buf.sem);
                }
            }
            return full ? CH_FULL : CH_OK;
        }
    }
}
#endif
