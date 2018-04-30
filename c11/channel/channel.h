/* Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef CHANNEL_H
#define CHANNEL_H
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CH_OK 0x0
#define CH_FULL 0x1
#define CH_EMPTY 0x2
#define CH_BUSY 0x4
#define CH_WBLOCK 0x8
#define CH_TIMEDOUT 0x10
#define CH_CLOSED 0x20

#define CH_OP_SEND 0x1
#define CH_OP_RECV 0x2

#define CH_SEL_INIT 0x0
#define CH_SEL_READY 0x1
#define CH_SEL_EXC 0x2
#define CH_SEL_CLOSED 0x4

typedef struct channel {
    size_t cap, write, read;
    pthread_mutex_t lock, wlock, rlock;
    pthread_cond_t empty, full;
    struct channel_select *sel;
    uint16_t refc, selid;
    bool closed, selwait;
    uint8_t selop;
    char buf[];
} channel;

typedef struct channel_select {
    pthread_mutex_t lock;
    pthread_cond_t init, ready, exc;
    uint16_t refc, id, len, cap;
    uint8_t state;
    struct channel_select_elt_ {
        channel *c;
        uint8_t op;
    } arr[];
} channel_select;

#define ch_make(type, cap) channel_make(sizeof(type), cap)
#define ch_dup(c) channel_dup(c)
#define ch_close(c) channel_close(c)
#define ch_drop(c) channel_drop(c)

#define ch_send(c, type, elt) channel_send(c, sizeof(type), &elt)
#define ch_trysend(c, type, elt) channel_trysend(c, sizeof(type), &elt)
#define ch_timedsend(c, type, offset, elt) \
    channel_timedsend(c, sizeof(type), offset, &elt)
#define ch_forcesend(c, type, elt) channel_forcesend(c, sizeof(type), &elt)

#define ch_recv(c, type, elt) channel_recv(c, sizeof(type), &elt)
#define ch_tryrecv(c, type, elt) channel_tryrecv(c, sizeof(type), &elt)
#define ch_timedrecv(c, type, offset, elt) \
    channel_timedrecv(c, sizeof(type), offset, &elt)

#define ch_sel_init(cap) channel_select_init(cap)
#define ch_sel_finish(sel) channel_select_finish(sel)
#define ch_sel_reg(sel, c, op) channel_select_reg(sel, c, op)
#define ch_select(sel) channel_select_do(sel)
#define ch_sel_send(c, type, elt) channel_select_send(c, sizeof(type), elt)
#define ch_sel_recv(c, type, elt) channel_select_recv(c, sizeof(type), &elt)

/* Polling alternative to ch_select. Loops over the cases (op is intended to be
 * either trysend or tryrecv) rather than using a condition variable which can
 * burn a lot of cycles if there is no default case and none of the channels
 * are readable or writeable */
#define ch_poll(casec) { \
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

#define ch_poll_end } }

/* These declarations must be present in exactly one compilation unit */
#define CHANNEL_EXTERN_DECL \
    extern inline channel *channel_make(size_t, size_t); \
    extern inline channel *channel_dup(channel *); \
    extern inline void channel_close(channel *); \
    extern inline channel *channel_drop(channel *); \
    extern inline bool channel_signal_select_(channel *, uint8_t); \
    extern inline int channel_send_buf_(channel *, size_t, void *); \
    extern inline int channel_send_unbuf_(channel *, size_t, void *); \
    extern inline int channel_send(channel *, size_t, void *); \
    extern inline int channel_trysend_buf_(channel *, size_t, void *); \
    extern inline int channel_trysend_unbuf_(channel *, size_t, void *); \
    extern inline int channel_trysend(channel *, size_t, void *); \
    extern inline struct timespec channel_add_offset_(long); \
    extern inline int channel_timedsend_buf_( \
        channel *, size_t, long, void *); \
    extern inline int channel_timedsend_unbuf_( \
        channel *, size_t, long, void *); \
    extern inline int channel_timedsend(channel *, size_t, long, void *); \
    extern inline int channel_forcesend(channel *, size_t, void *); \
    extern inline int channel_recv_buf_(channel *, size_t, void *); \
    extern inline int channel_recv_unbuf_(channel *, size_t, void *); \
    extern inline int channel_recv(channel *, size_t, void *); \
    extern inline int channel_tryrecv_buf_(channel *, size_t, void *); \
    extern inline int channel_tryrecv_unbuf_(channel *, size_t, void *); \
    extern inline int channel_tryrecv(channel *, size_t, void *); \
    extern inline int channel_timedrecv_buf_( \
        channel *, size_t, long, void *); \
    extern inline int channel_timedrecv_unbuf_( \
        channel *, size_t, long, void *); \
    extern inline int channel_timedrecv(channel *, size_t, long, void *); \
    extern inline channel_select *channel_select_init(uint16_t); \
    extern inline channel_select *channel_select_finish(channel_select *); \
    extern inline uint16_t channel_select_reg( \
            channel_select *, channel *, uint8_t); \
    extern inline uint16_t channel_select_do(channel_select *); \
    extern inline bool channel_select_valid_(channel *, uint8_t); \
    extern inline void channel_select_send(channel *, size_t, void *); \
    extern inline void channel_select_recv(channel *, size_t, void *)

/* If the capacity is 0 then the channel is unbuffered. Otherwise the channel
 * is buffered and the capacity must be a power of 2 */
inline channel *
channel_make(size_t eltsize, size_t cap) {
    channel *c = calloc(
        1, offsetof(channel, buf) + ((cap ? cap : 1) * eltsize));
    assert((cap & (cap - 1)) == 0 && c);
    c->cap = cap;
    c->lock = c->wlock = c->rlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    c->empty = c->full = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    c->refc = 1;
    return c;
}

/* Increases the reference count of the channel and returns it. Intended to be
 * used with multiple producers that independently close the channel */
inline channel *
channel_dup(channel *c) {
    pthread_mutex_lock(&c->lock);
    c->refc++;
    pthread_mutex_unlock(&c->lock);
    return c;
}

/* Closes the channel if the caller has the last reference to the channel.
 * Decreases the reference count otherwise */
inline void
channel_close(channel *c) {
    pthread_mutex_lock(&c->lock);
    assert(c->refc > 0);
    if (--c->refc == 0) {
        c->closed = true;
        if (c->cap == 0) {
            c->write = 0;
        }
        pthread_cond_broadcast(&c->empty);
        pthread_cond_broadcast(&c->full);
    }
    pthread_mutex_unlock(&c->lock);
}

/* Deallocates all resources associated with the channel */
inline channel *
channel_drop(channel *c) {
    assert(pthread_mutex_destroy(&c->lock) == 0 &&
               pthread_mutex_destroy(&c->wlock) == 0 &&
               pthread_mutex_destroy(&c->rlock) == 0 &&
               pthread_cond_destroy(&c->empty) == 0 &&
               pthread_cond_destroy(&c->full) == 0);
    free(c);
    return NULL;
}

inline bool
channel_signal_select_(channel *c, uint8_t op) {
    if (!c->sel || !(c->selop & op)) {
        return false;
    }

    channel_select *sel = c->sel;
    bool selected = false;
    pthread_mutex_lock(&sel->lock);
    c->selwait = true;
    sel->refc++;
    while (sel->state != CH_SEL_READY) {
        /* TODO: Totally broken right now */
        if (sel->state == CH_SEL_CLOSED) {
            c->sel = NULL;
            pthread_cond_signal(&sel->ready);
            goto cleanup;
        }
        pthread_cond_wait(&sel->init, &sel->lock);
    }
    selected = true;
    sel->id = c->selid;
    sel->state = CH_SEL_EXC;
    pthread_cond_signal(&sel->ready);
    pthread_cond_wait(&sel->exc, &sel->lock);
cleanup:
    c->selwait = false;
    sel->refc--;
    pthread_mutex_unlock(&sel->lock);
    return selected;
}

/* Blocking sends on buffered channels block while the buffer is full */
inline int
channel_send_buf_(channel *c, size_t eltsize, void *elt) {
    int rc = CH_OK;
    pthread_mutex_lock(&c->wlock);
    pthread_mutex_lock(&c->lock);
    while (c->write - c->read == c->cap) {
        if (c->closed) {
            rc = CH_CLOSED;
            goto cleanup;
        }
        pthread_cond_wait(&c->full, &c->lock);
    }
    if (c->closed) {
        rc = CH_CLOSED;
        goto cleanup;
    }

    memcpy(c->buf + ((c->write++ & (c->cap - 1)) * eltsize), elt, eltsize);
    if (!channel_signal_select_(c, CH_OP_RECV)) {
        pthread_cond_signal(&c->empty);
    }
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->wlock);
    return rc;
}

/* Blocking sends on unbuffered channels block until there is a receiver */
inline int
channel_send_unbuf_(channel *c, size_t eltsize, void *elt) {
    int rc;
    pthread_mutex_lock(&c->wlock);
    pthread_mutex_lock(&c->lock);
    if (c->closed) {
        rc = CH_CLOSED;
        goto cleanup;
    }

    memcpy(c->buf, elt, eltsize);
    c->write = 1;
    if (!channel_signal_select_(c, CH_OP_RECV)) {
        pthread_cond_signal(&c->empty);
        pthread_cond_wait(&c->full, &c->lock);
    }
    rc = c->closed ? CH_CLOSED : CH_OK;
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->wlock);
    return rc;
}

/* Blocking send. See channel_send_buf_ and channel_send_unbuf_ */
inline int
channel_send(channel *c, size_t eltsize, void *elt) {
    if (c->cap > 0) {
        return channel_send_buf_(c, eltsize, elt);
    }
    return channel_send_unbuf_(c, eltsize, elt);
}

/* Nonblocking sends on buffered channels fail if the buffer is full */
inline int
channel_trysend_buf_(channel *c, size_t eltsize, void *elt) {
    int rc = CH_OK;
    pthread_mutex_lock(&c->wlock);
    pthread_mutex_lock(&c->lock);
    if (c->closed) {
        rc = CH_CLOSED;
        goto cleanup;
    }
    if (c->write - c->read == c->cap) {
        rc = CH_FULL;
        goto cleanup;
    }

    memcpy(c->buf + ((c->write++ & (c->cap - 1)) * eltsize), elt, eltsize);
    pthread_cond_signal(&c->empty);
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->wlock);
    return rc;
}

/* Nonblocking sends on unbuffered channels fail if there is no receiver */
inline int
channel_trysend_unbuf_(channel *c, size_t eltsize, void *elt) {
    int rc = CH_OK;
    if (pthread_mutex_trylock(&c->wlock) == EBUSY) {
        return CH_BUSY;
    }
    pthread_mutex_lock(&c->lock);
    if (c->closed) {
        rc = CH_CLOSED;
        goto cleanup;
    }
    if (c->read == 0) {
        rc = CH_WBLOCK;
        goto cleanup;
    }

    memcpy(c->buf, elt, eltsize);
    c->write = 1;
    pthread_cond_signal(&c->empty);
    pthread_cond_wait(&c->full, &c->lock);
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->wlock);
    return rc;
}

/* Nonblocking send. See channel_trysend_buf_ and channel_trysend_unbuf_ */
inline int
channel_trysend(channel *c, size_t eltsize, void *elt) {
    if (c->cap > 0) {
        return channel_trysend_buf_(c, eltsize, elt);
    }
    return channel_trysend_unbuf_(c, eltsize, elt);
}

/* Unfortunately we have to use CLOCK_REALTIME as there is no way to use
 * CLOCK_MONOTIME with pthread_mutex_timedlock. */
inline struct timespec
channel_add_offset_(long offset) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += (offset % 1000) * 1000000;
    if (ts.tv_nsec > 1000000000) {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec++;
    }
    ts.tv_sec += (offset - (offset % 1000)) / 1000;
    return ts;
}

/* Timed receives on buffered channels fail if the buffer remains full for the
 * duration of the timeout, specified as an offset in milliseconds */
inline int
channel_timedsend_buf_(channel *c, size_t eltsize, long offset, void *elt) {
    struct timespec ts = channel_add_offset_(offset);
    int rc = CH_OK;
#if _POSIX_TIMEOUTS >= 200112L
    if (pthread_mutex_timedlock(&c->wlock, &ts) == ETIMEDOUT) {
        return CH_BUSY | CH_TIMEDOUT;
    }
    if (pthread_mutex_timedlock(&c->lock, &ts) == ETIMEDOUT) {
        pthread_mutex_unlock(&c->wlock);
        return CH_BUSY | CH_TIMEDOUT;
    }
#else
    pthread_mutex_lock(&c->wlock);
    pthread_mutex_lock(&c->lock);
#endif
    while (c->write - c->read == c->cap) {
        if (c->closed) {
            rc = CH_CLOSED;
            goto cleanup;
        }
        if (pthread_cond_timedwait(&c->full, &c->lock, &ts) == ETIMEDOUT) {
            rc = (c->closed ? CH_CLOSED : CH_FULL) | CH_TIMEDOUT;
            goto cleanup;
        }
    }
    if (c->closed) {
        rc = CH_CLOSED;
        goto cleanup;
    }

    memcpy(c->buf + ((c->write++ & (c->cap - 1)) * eltsize), elt, eltsize);
    pthread_cond_signal(&c->empty);
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->wlock);
    return rc;
}

/* Timed sends on unbuffered channels fail if no receiver appears for the
 * duration of the timeout, specified as an offset in milliseconds */
inline int
channel_timedsend_unbuf_(channel *c, size_t eltsize, long offset, void *elt) {
    struct timespec ts = channel_add_offset_(offset);
    int rc = CH_OK;
#if _POSIX_TIMEOUTS >= 200112L
    if (pthread_mutex_timedlock(&c->wlock, &ts) == ETIMEDOUT) {
        return CH_BUSY | CH_TIMEDOUT;
    }
    if (pthread_mutex_timedlock(&c->lock, &ts) == ETIMEDOUT) {
        pthread_mutex_unlock(&c->wlock);
        return CH_BUSY | CH_TIMEDOUT;
    }
#else
    if (pthread_mutex_trylock(&c->wlock) == EBUSY) {
        return CH_BUSY;
    }
    pthread_mutex_lock(&c->lock);
#endif
    if (c->closed) {
        rc = CH_CLOSED;
        goto cleanup;
    }

    memcpy(c->buf, elt, eltsize);
    c->write = 1;
    pthread_cond_signal(&c->empty);
    while (c->write == 1) {
        if (pthread_cond_timedwait(&c->full, &c->lock, &ts) == ETIMEDOUT) {
            rc = (c->closed ? CH_CLOSED : CH_WBLOCK) | CH_TIMEDOUT;
            c->write = 0;
            goto cleanup;
        }
    }
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->wlock);
    return rc;
}

/* Timed send. May behave poorly if pthread_mutex_timedlock is not present in
 * the system's implementation of pthreads. See channel_timedsend_buf_ and
 * channel_timedsend_unbuf_ */
inline int
channel_timedsend(channel *c, size_t eltsize, long offset, void *elt) {
    if (c->cap > 0) {
        return channel_timedsend_buf_(c, eltsize, offset, elt);
    }
    return channel_timedsend_unbuf_(c, eltsize, offset, elt);
}

/* Forced sends on buffered channels do not block and overwrite the oldest
 * message if the buffer is full. Forced sends are not possible with unbuffered
 * channels */
inline int
channel_forcesend(channel *c, size_t eltsize, void *elt) {
    int rc = CH_OK;
    assert(c->cap > 0);
    pthread_mutex_lock(&c->wlock);
    pthread_mutex_lock(&c->lock);
    if (c->closed) {
        rc = CH_CLOSED;
        goto cleanup;
    }

    memcpy(c->buf + ((c->write++ & (c->cap - 1)) * eltsize), elt, eltsize);
    if (c->write - c->read > c->cap) {
        rc = CH_FULL;
        c->read++;
    }
    pthread_cond_signal(&c->empty);
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->wlock);
    return rc;
}

/* Blocking receives on buffered channels block when the buffer is empty */
inline int
channel_recv_buf_(channel *c, size_t eltsize, void *elt) {
    pthread_mutex_lock(&c->rlock);
    pthread_mutex_lock(&c->lock);
    while (c->read == c->write) {
        if (c->closed) {
            pthread_mutex_unlock(&c->lock);
            pthread_mutex_unlock(&c->rlock);
            return CH_CLOSED;
        }
        pthread_cond_wait(&c->empty, &c->lock);
    }

    memcpy(elt, c->buf + ((c->read++ & (c->cap - 1)) * eltsize), eltsize);
    if (!channel_signal_select_(c, CH_OP_SEND)) {
        pthread_cond_signal(&c->full);
    }
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->rlock);
    return CH_OK;
}

/* Blocking receives on unbuffered channels block when there is no sender */
inline int
channel_recv_unbuf_(channel *c, size_t eltsize, void *elt) {
    pthread_mutex_lock(&c->rlock);
    pthread_mutex_lock(&c->lock);
    c->read = 1;
    while (c->write == 0) {
        if (c->closed) {
            pthread_mutex_unlock(&c->lock);
            pthread_mutex_unlock(&c->rlock);
            return CH_CLOSED;
        }
        pthread_cond_wait(&c->empty, &c->lock);
    }

    memcpy(elt, c->buf, eltsize);
    c->write = c->read = 0;
    if (!channel_signal_select_(c, CH_OP_SEND)) {
        pthread_cond_signal(&c->full);
    }
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->rlock);
    return CH_OK;
}

/* Blocking receive. See channel_recv_buf_ and channel_recv_unbuf_ */
inline int
channel_recv(channel *c, size_t eltsize, void *elt) {
    if (c->cap > 0) {
        return channel_recv_buf_(c, eltsize, elt);
    }
    return channel_recv_unbuf_(c, eltsize, elt);
}

/* Nonblocking receives on buffered channels fail if the buffer is empty */
inline int
channel_tryrecv_buf_(channel *c, size_t eltsize, void *elt) {
    int rc = CH_OK;
    pthread_mutex_lock(&c->rlock);
    pthread_mutex_lock(&c->lock);
    if (c->read == c->write) {
        rc = c->closed ? CH_CLOSED : CH_EMPTY;
        goto cleanup;
    } else if (c->closed) {
        rc = CH_CLOSED;
        goto cleanup;
    }

    memcpy(elt, c->buf + ((c->read++ & (c->cap - 1)) * eltsize), eltsize);
    pthread_cond_signal(&c->full);
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->rlock);
    return rc;
}

/* Nonblocking receives on unbuffered channels fail if there is no sender */
inline int
channel_tryrecv_unbuf_(channel *c, size_t eltsize, void *elt) {
    int rc = CH_OK;
    if (pthread_mutex_trylock(&c->rlock) == EBUSY) {
        return CH_BUSY;
    }
    pthread_mutex_lock(&c->lock);
    if (c->write == 0) {
        rc = c->closed ? CH_CLOSED : CH_WBLOCK;
        goto cleanup;
    } else if (c->closed) {
        rc = CH_CLOSED;
        goto cleanup;
    }

    memcpy(elt, c->buf, eltsize);
    c->write = 0;
    pthread_cond_signal(&c->full);
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->rlock);
    return rc;
}

/* Nonblocking receive. See channel_tryrecv_buf_ and channel_tryrecv_unbuf */
inline int
channel_tryrecv(channel *c, size_t eltsize, void *elt) {
    if (c->cap > 0) {
        return channel_tryrecv_buf_(c, eltsize, elt);
    }
    return channel_tryrecv_unbuf_(c, eltsize, elt);
}

/* Timed receives on buffered channels fail if the buffer remains empty for the
 * duration of the timeout, specified as an offset in milliseconds */
inline int
channel_timedrecv_buf_(channel *c, size_t eltsize, long offset, void *elt) {
    struct timespec ts = channel_add_offset_(offset);
    int rc = CH_OK;
#if _POSIX_TIMEOUTS >= 200112L
    if (pthread_mutex_timedlock(&c->rlock, &ts) == ETIMEDOUT) {
        return CH_BUSY | CH_TIMEDOUT;
    }
    if (pthread_mutex_timedlock(&c->lock, &ts) == ETIMEDOUT) {
        pthread_mutex_unlock(&c->wlock);
        return CH_BUSY | CH_TIMEDOUT;
    }
#else
    pthread_mutex_lock(&c->rlock);
    pthread_mutex_lock(&c->lock);
#endif
    while (c->read == c->write) {
        if (c->closed) {
            rc = CH_CLOSED;
            goto cleanup;
        }
        if (pthread_cond_timedwait(&c->empty, &c->lock, &ts) == ETIMEDOUT) {
            rc = (c->closed ? CH_CLOSED : CH_EMPTY) | CH_TIMEDOUT;
            goto cleanup;
        }
    }

    memcpy(elt, c->buf + ((c->read++ & (c->cap - 1)) * eltsize), eltsize);
    pthread_cond_signal(&c->full);
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->rlock);
    return rc;
}

/* Timed receives on unbuffered channels fail if no sender appears for the
 * duration of the timeout, specified as an offset in milliseconds */
inline int
channel_timedrecv_unbuf_(channel *c, size_t eltsize, long offset, void *elt) {
    struct timespec ts = channel_add_offset_(offset);
    int rc = CH_OK;
#if _POSIX_TIMEOUTS >= 200112L
    if (pthread_mutex_timedlock(&c->rlock, &ts) == ETIMEDOUT) {
        return CH_BUSY | CH_TIMEDOUT;
    }
    if (pthread_mutex_timedlock(&c->lock, &ts) == ETIMEDOUT) {
        pthread_mutex_unlock(&c->wlock);
        return CH_BUSY | CH_TIMEDOUT;
    }
#else
    if (pthread_mutex_trylock(&c->rlock) == EBUSY) {
        return CH_BUSY;
    }
    pthread_mutex_lock(&c->lock);
#endif
    while (c->write == 0) {
        if (c->closed) {
            rc = CH_CLOSED;
            goto cleanup;
        }
        if (pthread_cond_timedwait(&c->empty, &c->lock, &ts) == ETIMEDOUT) {
            rc = (c->closed ? CH_CLOSED : CH_WBLOCK) | CH_TIMEDOUT;
            goto cleanup;
        }
    }

    memcpy(elt, c->buf, eltsize);
    c->write = 0;
    pthread_cond_signal(&c->full);
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->rlock);
    return rc;
}

/* Timed receive. May behave poorly if pthread_mutex_timedlock is not present
 * in the system's implementation of pthreads. See channel_timedrecv_buf_ and
 * channel_timedrecv_unbuf_ */
inline int
channel_timedrecv(channel *c, size_t eltsize, long offset, void *elt) {
    if (c->cap > 0) {
        return channel_timedrecv_buf_(c, eltsize, offset, elt);
    }
    return channel_timedrecv_unbuf_(c, eltsize, offset, elt);
}

/* Experimental implementation of select. Totally broken */
inline channel_select *
channel_select_init(uint16_t cap) {
    channel_select *sel = malloc(
        offsetof(channel_select, arr) +
            (cap * sizeof(struct channel_select_elt_)));
    assert(cap != 0 && sel);
    sel->lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    sel->init = sel->ready = sel->exc =
        (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    sel->refc = sel->id = sel->len = 0;
    sel->cap = cap;
    sel->state = CH_SEL_INIT;
    return sel;
}

/* TODO: Totally broken right now */
inline channel_select *
channel_select_finish(channel_select *sel) {
    pthread_mutex_lock(&sel->lock);
    sel->state = CH_SEL_CLOSED;
    for (unsigned i = 0; i < sel->len; i++) {
        channel *c = sel->arr[i].c;
        if (pthread_mutex_trylock(&c->lock) == EBUSY) {
            continue;
        }
        c->sel = NULL;
        pthread_mutex_unlock(&c->lock);
    }
    while (sel->refc > 0) {
        pthread_cond_broadcast(&sel->init);
        pthread_cond_wait(&sel->ready, &sel->lock);
    }
    pthread_mutex_unlock(&sel->lock);
    assert(pthread_mutex_destroy(&sel->lock) == 0 &&
               pthread_cond_destroy(&sel->init) == 0 &&
               pthread_cond_destroy(&sel->ready) == 0 &&
               pthread_cond_destroy(&sel->exc) == 0);
    free(sel);
    return NULL;
}

inline uint16_t
channel_select_reg(channel_select *sel, channel *c, uint8_t op) {
    assert(sel->len < sel->cap && op != 0);
    pthread_mutex_lock(&c->lock);
    c->sel = sel;
    c->selid = sel->len;
    c->selop = op;
    pthread_mutex_unlock(&c->lock);
    sel->arr[sel->len] = (struct channel_select_elt_){c, op};
    return sel->len++;
}

inline bool
channel_select_valid_(channel *c, uint8_t op) {
    if (c->closed) {
        return false;
    }
    if ((op & CH_OP_SEND) &&
            ((c->cap == 0 && c->read == 1) ||
                (c->cap > 0 && c->write - c->read < c->cap))) {
        return true;
    }
    if ((c->cap == 0 && c->write == 1) || (c->cap > 0 && c->write > c->read)) {
        return true;
    }
    return false;
}

/* TODO: Acquire rlock if channel is unbuffered */
inline uint16_t
channel_select_do(channel_select *sel) {
    pthread_mutex_lock(&sel->lock);
    for (uint16_t i = 0; i < sel->len; i++) {
        channel *c = sel->arr[i].c;
        if (pthread_mutex_trylock(&c->lock) == EBUSY) {
            continue;
        }
        if (!c->selwait && channel_select_valid_(c, sel->arr[i].op)) {
            return i;
        }
        pthread_mutex_unlock(&c->lock);
    }
    sel->state = CH_SEL_READY;
    do {
        pthread_cond_signal(&sel->init);
        pthread_cond_wait(&sel->ready, &sel->lock);
        if (sel->state == CH_SEL_EXC) {
            channel *c = sel->arr[sel->id].c;
            if (c->selwait && channel_select_valid_(c, sel->arr[sel->id].op)) {
                return sel->id;
            }
            assert(false);
        }
        sel->state = CH_SEL_READY;
    } while (sel->state == CH_SEL_READY);
    assert(false);
}

/* Not yet implemented */
inline void
channel_select_send(channel *c, size_t eltsize, void *elt) {
    assert(false);
    if (c->cap > 0) {
        memcpy(c->buf + ((c->write++ & (c->cap - 1)) * eltsize), elt, eltsize);
        pthread_cond_signal(&c->empty);
    } else {
        memcpy(c->buf, elt, eltsize);
        c->write = 1;
        pthread_cond_signal(&c->empty);
        while (c->write == 1) {
            pthread_cond_wait(&c->full, &c->lock);
        }
    }
}

/* TODO: Release rlock if channel is unbuffered */
inline void
channel_select_recv(channel *c, size_t eltsize, void *elt) {
    if (!c->selwait) {
        if (c->cap > 0) {
            memcpy(
                elt, c->buf + ((c->read++ & (c->cap - 1)) * eltsize), eltsize);
            pthread_cond_signal(&c->full);
        } else {
            memcpy(elt, c->buf, eltsize);
            c->write = c->read = 0;
            pthread_cond_signal(&c->full);
        }
        pthread_mutex_unlock(&c->lock);
        goto cleanup;
    }

    if (c->cap > 0) {
        memcpy(
            elt, c->buf + ((c->read++ & (c->cap - 1)) * eltsize), eltsize);
    } else {
        memcpy(elt, c->buf, eltsize);
        c->write = c->read = 0;
    }
    c->sel->state = CH_SEL_INIT;
    pthread_cond_signal(&c->sel->exc);
cleanup:
    pthread_mutex_unlock(&c->sel->lock);
    return;
}
#endif
