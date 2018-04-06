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

#define CH_OK 0x0
#define CH_FULL 0x1
#define CH_EMPTY 0x2
#define CH_BUSY 0x4
#define CH_WBLOCK 0x8
#define CH_CLOSED 0x10

#define MACRO_CONCAT_(x, y) x##y
#define MC_(x, y) MACRO_CONCAT_(x, y)

#define channel(T) MC_(channel_, T)
#define channel_buf_(T) MC_(channel_buf_, T)
#define channel_unbuf_(T) MC_(channel_unbuf_, T)
#define PTR_OF(T) MC_(T, ptr)

#define CHANNEL_HDR_ \
    size_t cap, write, read; \
    pthread_mutex_t lock, wlock, rlock; \
    pthread_cond_t empty, full; \
    uint16_t refs; \
    bool closed

#define CHANNEL_DEF(T) \
    typedef struct channel_buf_(T) { \
        CHANNEL_HDR_; \
        T buf[]; \
    } channel_buf_(T); \
    typedef struct channel_unbuf_(T) { \
        CHANNEL_HDR_; \
        T elt; \
    } channel_unbuf_(T); \
    typedef union channel(T) { \
        struct { \
            CHANNEL_HDR_; \
        } hdr; \
        channel_buf_(T) buf; \
        channel_unbuf_(T) unbuf; \
    } channel(T)
#define CHANNEL_DEF_PTR(T) \
    typedef T *PTR_OF(T); \
    CHANNEL_DEF(PTR_OF(T))

#define ch_make(T, cap_) __extension__({ \
    __auto_type capX_ = cap_; \
    channel(T) *cX_; \
    if (capX_ > 0) { \
        cX_ = malloc(offsetof(channel_buf_(T), buf) + \
                         (capX_ * sizeof(T))); \
        assert((capX_ & (capX_ - 1)) == 0 && cX_); \
    } else { \
        assert(cX_ = malloc(sizeof(channel_unbuf_(T)))); \
    } \
    cX_->hdr.cap = capX_; \
    cX_->hdr.write = cX_->hdr.read = 0; \
    cX_->hdr.lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER; \
    cX_->hdr.wlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER; \
    cX_->hdr.rlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER; \
    cX_->hdr.empty = (pthread_cond_t)PTHREAD_COND_INITIALIZER; \
    cX_->hdr.full = (pthread_cond_t)PTHREAD_COND_INITIALIZER; \
    cX_->hdr.refs = 1; \
    cX_->hdr.closed = false; \
    cX_; })

#define ch_dup(chan) __extension__({ \
    __extension__ __auto_type cX_ = chan; \
    cX_->hdr.refs++; \
    cX_; })

#define ch_close(chan) do { \
    __extension__ __auto_type cX_ = chan; \
    pthread_mutex_lock(&cX_->hdr.lock); \
    assert(cX_->hdr.refs > 0); \
    if (--cX_->hdr.refs == 0) { \
        cX_->hdr.closed = true; \
        pthread_cond_broadcast(&cX_->hdr.empty); \
        pthread_cond_broadcast(&cX_->hdr.full); \
    } \
    pthread_mutex_unlock(&cX_->hdr.lock); } while (0)

#define ch_forceclose(chan) do { \
    __extension__ __auto_type cX_ = chan; \
    pthread_mutex_lock(&cX_->hdr.lock); \
    assert(cX_->hdr.refs > 0); \
    cX_->hdr.closed = true; \
    pthread_cond_broadcast(&cX_->hdr.empty); \
    pthread_cond_broadcast(&cX_->hdr.full); \
    pthread_mutex_unlock(&cX_->hdr.lock); } while (0)

#define ch_drop(chan) __extension__({ \
    __auto_type cX_ = chan; \
    assert(pthread_mutex_destroy(&cX_->hdr.lock) == 0); \
    assert(pthread_mutex_destroy(&cX_->hdr.wlock) == 0); \
    assert(pthread_mutex_destroy(&cX_->hdr.rlock) == 0); \
    assert(pthread_cond_destroy(&cX_->hdr.empty) == 0); \
    assert(pthread_cond_destroy(&cX_->hdr.full) == 0); \
    free(cX_); \
    NULL; })

#define ch_send(chan, elt_) __extension__({ \
    __auto_type cX_ = chan; \
    int retX_ = CH_OK; \
    pthread_mutex_lock(&cX_->hdr.wlock); \
    pthread_mutex_lock(&cX_->hdr.lock); \
    if (cX_->hdr.cap > 0) { \
        while (cX_->buf.write - cX_->buf.read == cX_->buf.cap) { \
            pthread_cond_wait(&cX_->buf.full, &cX_->buf.lock); \
        } \
        if (cX_->buf.closed) { \
            retX_ = CH_CLOSED; \
        } else { \
            cX_->buf.buf[cX_->buf.write++ & (cX_->buf.cap - 1)] = elt_; \
            pthread_cond_signal(&cX_->buf.empty); \
        } \
    } else { \
        if (cX_->unbuf.closed) { \
            retX_ = CH_CLOSED; \
        } else { \
            cX_->unbuf.elt = elt_; \
            cX_->unbuf.write = 1; \
            pthread_cond_signal(&cX_->unbuf.empty); \
            pthread_cond_wait(&cX_->unbuf.full, &cX_->unbuf.lock); \
            cX_->unbuf.read = 0; \
        } \
    } \
    pthread_mutex_unlock(&cX_->hdr.lock); \
    pthread_mutex_unlock(&cX_->hdr.wlock); \
    retX_; })

#define ch_trysend(chan, elt_) __extension__({ \
    __auto_type cX_ = chan; \
    int retX_ = CH_OK; \
    if (cX_->hdr.cap > 0) { \
        pthread_mutex_lock(&cX_->buf.lock); \
        if (cX_->buf.closed) { \
            retX_ = CH_CLOSED; \
        } else if (cX_->buf.write - cX_->buf.read == cX_->buf.cap) { \
            retX_ = CH_FULL; \
        } else { \
            cX_->buf.buf[cX_->buf.write++ & (cX_->buf.cap - 1)] = elt_; \
            pthread_cond_signal(&cX_->buf.empty); \
        } \
        pthread_mutex_unlock(&cX_->buf.lock); \
    } else { \
        if (pthread_mutex_trylock(&cX_->unbuf.wlock) == EBUSY) { \
            retX_ = CH_BUSY; \
        } else { \
            pthread_mutex_lock(&cX_->unbuf.lock); \
            if (cX_->unbuf.closed) { \
                retX_ = CH_CLOSED; \
            } else if (cX_->unbuf.read == 0) { \
                retX_ = CH_WBLOCK; \
            } else { \
                cX_->unbuf.elt = elt_; \
                cX_->unbuf.write = 1; \
                pthread_cond_signal(&cX_->unbuf.empty); \
                pthread_cond_wait(&cX_->unbuf.full, &cX_->unbuf.lock); \
                cX_->unbuf.read = 0; \
            } \
            pthread_mutex_unlock(&cX_->unbuf.lock); \
            pthread_mutex_unlock(&cX_->unbuf.wlock); \
        } \
    } \
    retX_; })

#define ch_forcesend(chan, elt_) __extension__({ \
    __auto_type cX_ = chan; \
    int retX_ = CH_OK; \
    pthread_mutex_lock(&cX_->hdr.wlock); \
    pthread_mutex_lock(&cX_->hdr.lock); \
    assert(cX_->hdr.cap > 0); \
    if (cX_->buf.closed) { \
        retX_ = CH_CLOSED; \
    } else { \
        cX_->buf.buf[cX_->buf.write++ & (cX_->buf.cap - 1)] = elt_; \
        if (cX_->buf.write - cX_->buf.read > cX_->buf.cap) { \
            retX_ = CH_FULL; \
            cX_->buf.read++; \
        } \
        pthread_cond_signal(&cX_->buf.empty); \
    } \
    pthread_mutex_unlock(&cX_->hdr.lock); \
    pthread_mutex_unlock(&cX_->hdr.wlock); \
    retX_; })

#define ch_recv(chan, elt_) __extension__({ \
    __auto_type cX_ = chan; \
    int retX_ = CH_OK; \
    pthread_mutex_lock(&cX_->hdr.rlock); \
    pthread_mutex_lock(&cX_->hdr.lock); \
    if (cX_->hdr.cap > 0) { \
        while (cX_->buf.read == cX_->buf.write) { \
            if (cX_->buf.closed) { \
                retX_ = CH_CLOSED; \
                break; \
            } \
            pthread_cond_wait(&cX_->buf.empty, &cX_->buf.lock); \
        } \
        if (retX_ == CH_OK) { \
            elt_ = cX_->buf.buf[cX_->buf.read++ & (cX_->buf.cap - 1)]; \
            pthread_cond_signal(&cX_->buf.full); \
        } \
    } else { \
        cX_->unbuf.read = 1; \
        while (cX_->unbuf.write == 0) { \
            if (cX_->unbuf.closed) { \
                retX_ = CH_CLOSED; \
                break; \
            } \
            pthread_cond_wait(&cX_->unbuf.empty, &cX_->unbuf.lock); \
        } \
        if (retX_ == CH_OK) { \
            elt_ = cX_->unbuf.elt; \
            cX_->unbuf.write = 0; \
            pthread_cond_signal(&cX_->unbuf.full); \
        } \
    } \
    pthread_mutex_unlock(&cX_->hdr.lock); \
    pthread_mutex_unlock(&cX_->hdr.rlock); \
    retX_; })

#define ch_tryrecv(chan, elt_) __extension__({ \
    __auto_type cX_ = chan; \
    int retX_ = CH_OK; \
    if (cX_->hdr.cap > 0) { \
        pthread_mutex_lock(&cX_->buf.lock); \
        if (cX_->buf.closed) { \
            retX_ = CH_CLOSED; \
        } else if (cX_->buf.read == cX_->buf.write) { \
            retX_ = CH_EMPTY; \
        } else { \
            elt_ = cX_->buf.buf[cX_->buf.read++ & (cX_->buf.cap - 1)]; \
            pthread_cond_signal(&cX_->buf.full); \
        } \
        pthread_mutex_unlock(&cX_->buf.lock); \
    } else { \
        if (pthread_mutex_trylock(&cX_->unbuf.rlock) == EBUSY) { \
            retX_ = CH_BUSY; \
        } else { \
            pthread_mutex_lock(&cX_->unbuf.lock); \
            if (cX_->unbuf.closed) { \
                retX_ = CH_CLOSED; \
            } else if (cX_->unbuf.write == 0) { \
                retX_ = CH_WBLOCK; \
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

#define ch_select(num) { \
    bool doneX_ = false; \
    switch (rand() % num) { \
    for ( ; ; doneX_ = true)

#define ch_case(id, op, ...) \
    case id: \
    if (op == CH_OK) { \
        __VA_ARGS__; \
        break; \
    } else (void)0

#define ch_default(...) \
    if (doneX_) { \
        __VA_ARGS__; \
        break; \
    } else (void)0

#define ch_select_end } }
#endif
