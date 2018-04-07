/* Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef RINGBUF_H
#define RINGBUF_H
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#ifndef MINIMAL_
#define MINIMAL_
#define MACRO_CONCAT_(x, y) x##y
#define MC_(x, y) MACRO_CONCAT_(x, y)
#define PTR_OF(T) MC_(T, ptr)
#endif

#define ringbuf(T) MC_(ringbuf_, T)

#define RINGBUF_DEF(T) \
    typedef struct ringbuf(T) { \
        size_t cap, write; \
        atomic_size_t read; \
        pthread_rwlock_t lock; \
        T buf[]; \
    } ringbuf(T)
#define RINGBUF_DEF_PTR(T) \
    typedef T *PTR_OF(T); \
    RINGBUF_DEF(PTR_OF(T))

#define LOAD_RLX_(obj) atomic_load_explicit(obj, memory_order_relaxed)

#define rbuf_make(T, cap_) __extension__({ \
    __auto_type cX_ = cap_; \
    ringbuf(T) *rX_ = malloc(offsetof(ringbuf(T), buf) + (cX_ * sizeof(T))); \
    assert(cX_  > 0 && (cX_ & (cX_ - 1)) == 0 && rX_->buf); \
    rX_->write = 0; \
    atomic_store_explicit(&rX_->read, 0, memory_order_relaxed); \
    rX_->cap = cX_; \
    rX_->lock = (pthread_rwlock_t)PTHREAD_RWLOCK_INITIALIZER; \
    rX_; })

#define rbuf_drop(rbuf) __extension__({ \
    __auto_type rX_ = rbuf; \
    assert(pthread_rwlock_destroy(&rX_->lock) == 0); \
    free(rX_); \
    NULL; })

#define rbuf_push(rbuf, elt) __extension__({ \
    __auto_type rX_ = rbuf; \
    pthread_rwlock_wrlock(&rX_->lock); \
    rX_->buf[rX_->write++ & (rX_->cap - 1)] = elt; \
    ssize_t diffX_ = rX_->write - LOAD_RLX_(&rX_->read) - rX_->cap; \
    if (diffX_ > 0) { \
        atomic_fetch_add_explicit(&rX_->read, 1, memory_order_relaxed); \
    } \
    pthread_rwlock_unlock(&rX_->lock); \
    diffX_ < 0; })

#define rbuf_trypush(rbuf, elt) __extension__({ \
    __auto_type rX_ = rbuf; \
    pthread_rwlock_wrlock(&rX_->lock); \
    bool retX_ = rX_->write - LOAD_RLX_(&rX_->read) < rX_->cap; \
    if (retX_) { \
        rX_->buf[rX_->write++ & (rX_->cap - 1)] = elt; \
    } \
    pthread_rwlock_unlock(&rX_->lock); \
    retX_; })

#define rbuf_shift(rbuf, elt) __extension__({ \
    __auto_type rX_ = rbuf; \
    pthread_rwlock_rdlock(&rX_->lock); \
    size_t iX_; \
    bool retX_; \
    for ( ; ; ) { \
        iX_ = atomic_load_explicit(&rX_->read, memory_order_acquire); \
        retX_ = iX_ != rX_->write; \
        if (retX_) { \
            if (atomic_compare_exchange_weak(&rX_->read, &iX_, iX_ + 1)) { \
                elt = rX_->buf[iX_ & (rX_->cap - 1)]; \
                break; \
            } \
        } else { \
            break; \
        } \
    } \
    pthread_rwlock_unlock(&rX_->lock); \
    retX_; })

#define rbuf_peek(rbuf, elt) __extension__({ \
    __auto_type rX_ = rbuf; \
    pthread_rwlock_rdlock(&rX_->lock); \
    size_t iX_ = atomic_load_explicit(&rX_->read, memory_order_acquire); \
    bool retX_ = iX_ != rX_->write; \
    if (retX_) { \
        elt = rX_->buf[iX_ & (rX_->cap - 1)]; \
    } \
    pthread_rwlock_unlock(&rX_->lock); \
    retX_; })

#endif
