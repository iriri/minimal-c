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

typedef struct channel {
    size_t cap, write, read;
    pthread_mutex_t lock, wlock, rlock;
    pthread_cond_t empty, full;
    uint16_t refs;
    bool closed;
    char buf[];
} channel;

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

#define CHANNEL_EXTERN_DECL \
    extern inline channel *channel_make(size_t, size_t); \
    extern inline channel *channel_dup(channel *); \
    extern inline void channel_close(channel *); \
    extern inline channel *channel_drop(channel *); \
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
    extern inline int channel_timedrecv(channel *, size_t, long, void *)

#define ch_select(num) { \
    bool Xdone_ = false; \
    switch (rand() % num) { \
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

#define ch_select_end } }

/* If the capacity is 0 then the channel is unbuffered. Otherwise the channel
 * is buffered and the capacity must be a power of 2 */
inline channel *
channel_make(size_t eltsize, size_t cap) {
    channel *c = calloc(
        1, offsetof(channel, buf) + ((cap ? cap : 1) * eltsize));
    assert((cap & (cap - 1)) == 0 && c);
    c->cap = cap;
    c->lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    c->wlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    c->rlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    c->empty = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    c->full = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    c->refs = 1;
    return c;
}

/* Increases the reference count of the channel and returns it. Intended to be
 * used with multiple producers that independently close the channel */
inline channel *
channel_dup(channel *c) {
    pthread_mutex_lock(&c->lock);
    c->refs++;
    pthread_mutex_unlock(&c->lock);
    return c;
}

/* Closes the channel if the caller has the last reference to the channel.
 * Decreases the reference count otherwise */
inline void
channel_close(channel *c) {
    pthread_mutex_lock(&c->lock);
    assert(c->refs > 0);
    if (--c->refs == 0) {
        c->closed = true;
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
    pthread_cond_signal(&c->empty);
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
    pthread_cond_signal(&c->empty);
    pthread_cond_wait(&c->full, &c->lock);
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
    if (pthread_cond_timedwait(&c->full, &c->lock, &ts) == ETIMEDOUT) {
        rc = (c->closed ? CH_CLOSED : CH_WBLOCK) | CH_TIMEDOUT;
        c->write = 0;
        goto cleanup;
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
    pthread_cond_signal(&c->full);
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
    pthread_cond_signal(&c->full);
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
#endif
