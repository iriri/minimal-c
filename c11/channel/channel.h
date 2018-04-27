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

#define CH_OK 0x0
#define CH_FULL 0x1
#define CH_EMPTY 0x2
#define CH_BUSY 0x4
#define CH_WBLOCK 0x8
#define CH_CLOSED 0x10

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
#define ch_forceclose(c) channel_forceclose(c)
#define ch_drop(c) channel_drop(c)
#define ch_send(c, type, elt) channel_send(c, sizeof(type), &elt)
#define ch_trysend(c, type, elt) channel_trysend(c, sizeof(type), &elt)
#define ch_forcesend(c, type, elt) channel_forcesend(c, sizeof(type), &elt)
#define ch_recv(c, type, elt) channel_recv(c, sizeof(type), &elt)
#define ch_tryrecv(c, type, elt) channel_tryrecv(c, sizeof(type), &elt)

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

#define CHANNEL_EXTERN_DECL \
    extern inline channel *channel_make(size_t, size_t); \
    extern inline channel *channel_dup(channel *); \
    extern inline void channel_close(channel *); \
    extern inline void channel_forceclose(channel *); \
    extern inline channel *channel_drop(channel *); \
    extern inline int channel_send_buf_(channel *, size_t, void *); \
    extern inline int channel_send_unbuf_(channel *, size_t, void *); \
    extern inline int channel_send(channel *, size_t, void *); \
    extern inline int channel_trysend_buf_(channel *, size_t, void *); \
    extern inline int channel_trysend_unbuf_(channel *, size_t, void *); \
    extern inline int channel_trysend(channel *, size_t, void *); \
    extern inline int channel_forcesend(channel *, size_t, void *); \
    extern inline int channel_recv_buf_(channel *, size_t, void *); \
    extern inline int channel_recv_unbuf_(channel *, size_t, void *); \
    extern inline int channel_recv(channel *, size_t, void *); \
    extern inline int channel_tryrecv_buf_(channel *, size_t, void *); \
    extern inline int channel_tryrecv_unbuf_(channel *, size_t, void *); \
    extern inline int channel_tryrecv(channel *, size_t, void *)

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

inline channel *
channel_dup(channel *c) {
    c->refs++;
    return c;
}

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

inline void
channel_forceclose(channel *c) {
    pthread_mutex_lock(&c->lock);
    assert(c->refs > 0);
    c->closed = true;
    pthread_cond_broadcast(&c->empty);
    pthread_cond_broadcast(&c->full);
    pthread_mutex_unlock(&c->lock);
}

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

inline int
channel_send_buf_(channel *c, size_t eltsize, void *elt) {
    pthread_mutex_lock(&c->wlock);
    pthread_mutex_lock(&c->lock);
    if (c->closed) {
        pthread_mutex_unlock(&c->lock);
        pthread_mutex_unlock(&c->wlock);
        return CH_CLOSED;
    }

    while (c->write - c->read == c->cap) {
        pthread_cond_wait(&c->full, &c->lock);
    }
    memcpy(c->buf + ((c->write++ & (c->cap - 1)) * eltsize), elt, eltsize);
    pthread_cond_signal(&c->empty);
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->wlock);
    return CH_OK;
}

inline int
channel_send_unbuf_(channel *c, size_t eltsize, void *elt) {
    pthread_mutex_lock(&c->wlock);
    pthread_mutex_lock(&c->lock);
    if (c->closed) {
        pthread_mutex_unlock(&c->lock);
        pthread_mutex_unlock(&c->wlock);
        return CH_CLOSED;
    }

    memcpy(c->buf, elt, eltsize);
    c->write = 1;
    pthread_cond_signal(&c->empty);
    pthread_cond_wait(&c->full, &c->lock);
    c->read = 0;
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->wlock);
    return CH_OK;
}

inline int
channel_send(channel *c, size_t eltsize, void *elt) {
    if (c->cap > 0) {
        return channel_send_buf_(c, eltsize, elt);
    }
    return channel_send_unbuf_(c, eltsize, elt);
}

inline int
channel_trysend_buf_(channel *c, size_t eltsize, void *elt) {
    pthread_mutex_lock(&c->lock);
    if (c->closed) {
        pthread_mutex_unlock(&c->lock);
        return CH_CLOSED;
    }
    if (c->write - c->read == c->cap) {
        pthread_mutex_unlock(&c->lock);
        return CH_FULL;
    }

    memcpy(c->buf + ((c->write++ & (c->cap - 1)) * eltsize), elt, eltsize);
    pthread_cond_signal(&c->empty);
    pthread_mutex_unlock(&c->lock);
    return CH_OK;
}

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
    c->read = 0;
cleanup:
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->wlock);
    return rc;
}

inline int
channel_trysend(channel *c, size_t eltsize, void *elt) {
    if (c->cap > 0) {
        return channel_trysend_buf_(c, eltsize, elt);
    }
    return channel_trysend_unbuf_(c, eltsize, elt);
}

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
    c->write = 0;
    pthread_cond_signal(&c->full);
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_unlock(&c->rlock);
    return CH_OK;
}

inline int
channel_recv(channel *c, size_t eltsize, void *elt) {
    if (c->cap > 0) {
        return channel_recv_buf_(c, eltsize, elt);
    }
    return channel_recv_unbuf_(c, eltsize, elt);
}

inline int
channel_tryrecv_buf_(channel *c, size_t eltsize, void *elt) {
    pthread_mutex_lock(&c->lock);
    if (c->closed) {
        pthread_mutex_unlock(&c->lock);
        return CH_CLOSED;
    }
    if (c->read == c->write) {
        pthread_mutex_unlock(&c->lock);
        return CH_EMPTY;
    }

    memcpy(elt, c->buf + ((c->read++ & (c->cap - 1)) * eltsize), eltsize);
    pthread_cond_signal(&c->full);
    pthread_mutex_unlock(&c->lock);
    return CH_OK;
}

inline int
channel_tryrecv_unbuf_(channel *c, size_t eltsize, void *elt) {
    int rc = CH_OK;
    if (pthread_mutex_trylock(&c->rlock) == EBUSY) {
        return CH_BUSY;
    }
    pthread_mutex_lock(&c->lock);
    if (c->closed) {
        rc = CH_CLOSED;
        goto cleanup;
    }
    if (c->write == 0) {
        rc = CH_WBLOCK;
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

inline int
channel_tryrecv(channel *c, size_t eltsize, void *elt) {
    if (c->cap > 0) {
        return channel_tryrecv_buf_(c, eltsize, elt);
    }
    return channel_tryrecv_unbuf_(c, eltsize, elt);
}
#endif
