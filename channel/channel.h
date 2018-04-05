/* Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef CHANNEL_H
#define CHANNEL_H
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define CHANNEL_OK 0
#define CHANNEL_FULL -1
#define CHANNEL_EMPTY -2
#define CHANNEL_BUSY -2
#define CHANNEL_CLOSED -4

#define MACRO_CONCAT_(x, y) x##y
#define MC_(x, y) MACRO_CONCAT_(x, y)

#define channel(T) MC_(channel_, T)
#define channel_buf_(T) MC_(channel_buf_, T)
#define channel_unbuf_(T) MC_(channel_unbuf_, T)
#define PTR_OF(T) MC_(T, ptr)

#define channel_hdr_ \
    size_t cap, write; \
    pthread_mutex_t lock; \
    pthread_cond_t empty; \
    pthread_cond_t full; \
    bool closed

#define CHANNEL_DEF(T) \
    typedef struct channel_buf_(T) { \
        channel_hdr_; \
        size_t read; \
        T buf[]; \
    } channel_buf_(T); \
    typedef struct channel_unbuf_(T) { \
        channel_hdr_; \
        pthread_mutex_t wlock; \
        pthread_mutex_t rlock; \
        T elt; \
    } channel_unbuf_(T); \
    typedef union channel(T) { \
        struct { \
            channel_hdr_; \
        } hdr; \
        channel_buf_(T) buf; \
        channel_unbuf_(T) unbuf; \
    } channel(T)
#define CHANNEL_DEF_PTR(T) \
    typedef T *PTR_OF(T); \
    CHANNEL_DEF(PTR_OF(T))

#define CHANNEL_MAKE(T, cap_) __extension__({ \
    __auto_type capX_ = cap_; \
    channel(T) *cX_; \
    if (capX_ > 0) { \
        cX_ = malloc(offsetof(channel_buf_(T), buf) + \
                         (capX_ * sizeof(T))); \
        assert(((cX_->hdr.cap = capX_) & (capX_ - 1)) == 0 && cX_); \
        cX_->buf.read = 0; \
    } else { \
        assert(cX_ = malloc(sizeof(channel_unbuf_(T)))); \
        cX_->unbuf.cap = 0; \
        cX_->unbuf.wlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER; \
        cX_->unbuf.rlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER; \
    } \
    cX_->hdr.write = 0; \
    cX_->hdr.lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER; \
    cX_->hdr.empty = (pthread_cond_t)PTHREAD_COND_INITIALIZER; \
    cX_->hdr.full = (pthread_cond_t)PTHREAD_COND_INITIALIZER; \
    cX_->hdr.closed = false; \
    cX_; })

#define CHANNEL_CLOSE(chan) do { \
    __extension__ __auto_type cX_ = chan; \
    pthread_mutex_lock(&cX_->hdr.lock); \
    cX_->hdr.closed = true; \
    cX_->hdr.write = 1; \
    if (cX_->hdr.cap > 0) { \
        cX_->buf.cap = 2; \
        cX_->buf.read = 0; \
    } \
    pthread_cond_broadcast(&cX_->hdr.empty); \
    pthread_cond_broadcast(&cX_->hdr.full); \
    pthread_mutex_unlock(&cX_->hdr.lock); } while (0)

#define CHANNEL_DROP(chan) __extension__({ \
    __auto_type cX_ = chan; \
    assert(pthread_mutex_destroy(&cX_->hdr.lock) == 0); \
    assert(pthread_cond_destroy(&cX_->hdr.empty) == 0); \
    assert(pthread_cond_destroy(&cX_->hdr.full) == 0); \
    if (cX_->hdr.cap == 0) { \
        assert(pthread_mutex_destroy(&cX_->unbuf.wlock) == 0); \
        assert(pthread_mutex_destroy(&cX_->unbuf.rlock) == 0); \
    } \
    free(cX_); \
    NULL; })

#define CHANNEL_PUSH(chan, elt_) __extension__({ \
    __auto_type cX_ = chan; \
    int retX_ = CHANNEL_OK; \
    if (cX_->hdr.cap > 0) { \
        pthread_mutex_lock(&cX_->buf.lock); \
        while (cX_->buf.write - cX_->buf.read == cX_->buf.cap) { \
            pthread_cond_wait(&cX_->buf.full, &cX_->buf.lock); \
        } \
        if (cX_->buf.closed) { \
            retX_ = CHANNEL_CLOSED; \
        } else { \
            cX_->buf.buf[cX_->buf.write++ & (cX_->buf.cap - 1)] = elt_; \
            pthread_cond_signal(&cX_->buf.empty); \
        } \
        pthread_mutex_unlock(&cX_->buf.lock); \
    } else { \
        pthread_mutex_lock(&cX_->unbuf.wlock); \
        pthread_mutex_lock(&cX_->unbuf.lock); \
        if (cX_->unbuf.closed) { \
            retX_ = CHANNEL_CLOSED; \
        } else { \
            cX_->unbuf.elt = elt_; \
            cX_->unbuf.write = 1; \
            pthread_cond_signal(&cX_->unbuf.empty); \
            pthread_cond_wait(&cX_->unbuf.full, &cX_->unbuf.lock); \
        } \
        pthread_mutex_unlock(&cX_->unbuf.lock); \
        pthread_mutex_unlock(&cX_->unbuf.wlock); \
    } \
    retX_; })

#define CHANNEL_TRYPUSH(chan, elt_) __extension__({ \
    __auto_type cX_ = chan; \
    int retX_ = CHANNEL_OK; \
    if (cX_->hdr.cap > 0) { \
        pthread_mutex_lock(&cX_->buf.lock); \
        if (cX_->buf.closed) { \
            retX_ = CHANNEL_CLOSED; \
        } else if (cX_->buf.write - cX_->buf.read == cX_->buf.cap) { \
            retX_ = CHANNEL_FULL; \
        } else { \
            cX_->buf.buf[cX_->buf.write++ & (cX_->buf.cap - 1)] = elt_; \
            pthread_cond_signal(&cX_->buf.empty); \
        } \
        pthread_mutex_unlock(&cX_->buf.lock); \
    } else { \
        if (pthread_mutex_trylock(&cX_->unbuf.wlock) == EBUSY) { \
            retX_ = CHANNEL_BUSY; \
        } else { \
            pthread_mutex_lock(&cX_->unbuf.lock); \
            if (cX_->unbuf.closed) { \
                retX_ = CHANNEL_CLOSED; \
            } else { \
                cX_->unbuf.elt = elt_; \
                cX_->unbuf.write = 1; \
                pthread_cond_signal(&cX_->unbuf.empty); \
                pthread_cond_wait(&cX_->unbuf.full, &cX_->unbuf.lock); \
            } \
            pthread_mutex_unlock(&cX_->unbuf.lock); \
            pthread_mutex_unlock(&cX_->unbuf.wlock); \
        } \
    } \
    retX_; })

#define CHANNEL_POP(chan, elt_) __extension__({ \
    __auto_type cX_ = chan; \
    int retX_ = CHANNEL_OK; \
    if (cX_->hdr.cap > 0) { \
        pthread_mutex_lock(&cX_->buf.lock); \
        while (cX_->buf.read == cX_->buf.write) { \
            pthread_cond_wait(&cX_->buf.empty, &cX_->buf.lock); \
        } \
        if (cX_->buf.closed) { \
            retX_ = CHANNEL_CLOSED; \
        } else { \
            elt_ = cX_->buf.buf[cX_->buf.read++ & (cX_->buf.cap - 1)]; \
            pthread_cond_signal(&cX_->buf.full); \
        } \
        pthread_mutex_unlock(&cX_->buf.lock); \
    } else { \
        pthread_mutex_lock(&cX_->unbuf.rlock); \
        pthread_mutex_lock(&cX_->unbuf.lock); \
        while (cX_->unbuf.write == 0) { \
            pthread_cond_wait(&cX_->unbuf.empty, &cX_->unbuf.lock); \
        } \
        if (cX_->unbuf.closed) { \
            retX_ = CHANNEL_CLOSED; \
        } else { \
            elt_ = cX_->unbuf.elt; \
            cX_->unbuf.write = 0; \
            pthread_cond_signal(&cX_->unbuf.full); \
        } \
        pthread_mutex_unlock(&cX_->unbuf.lock); \
        pthread_mutex_unlock(&cX_->unbuf.rlock); \
    } \
    retX_; })

#define CHANNEL_TRYPOP(chan, elt_) __extension__({ \
    __auto_type cX_ = chan; \
    int retX_ = CHANNEL_OK; \
    if (cX_->hdr.cap > 0) { \
        pthread_mutex_lock(&cX_->buf.lock); \
        if (cX_->buf.closed) { \
            retX_ = CHANNEL_CLOSED; \
        } else if (cX_->buf.read == cX_->buf.write) { \
            retX_ = CHANNEL_EMPTY; \
        } else { \
            elt_ = cX_->buf.buf[cX_->buf.read++ & (cX_->buf.cap - 1)]; \
            pthread_cond_signal(&cX_->buf.full); \
        } \
        pthread_mutex_unlock(&cX_->buf.lock); \
    } else { \
        if (pthread_mutex_trylock(&cX_->unbuf.rlock) == EBUSY) { \
            retX_ = CHANNEL_BUSY; \
        else { \
            pthread_mutex_lock(&cX_->unbuf.lock); \
            while (cX_->unbuf.write == 0) { \
                pthread_cond_wait(&cX_->unbuf.empty, &cX_->unbuf.lock); \
            } \
            if (cX_->unbuf.closed) { \
                retX_ = CHANNEL_CLOSED; \
            } else { \
                elt_ = cX_->unbuf.elt; \
                cX_->unbuf.write = 0; \
                pthread_cond_signal(&cX_->unbuf.full); \
            } \
            pthread_mutex_unlock(&cX_->unbuf.lock); \
            pthread_mutex_unlock(&cX_->unbuf.rlock); \
        } \
    } \
    retX_; })

/* #define CHANNEL_SELECT(...) */

#endif
