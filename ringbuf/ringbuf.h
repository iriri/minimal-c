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
    __auto_type Xcap_ = cap_; \
    ringbuf(T) *Xr_ = \
        calloc(1, offsetof(ringbuf(T), buf) + (Xcap_ * sizeof(T))); \
    assert(Xcap_  > 0 && (Xcap_ & (Xcap_ - 1)) == 0 && Xr_->buf); \
    Xr_->cap = Xcap_; \
    Xr_->lock = (pthread_rwlock_t)PTHREAD_RWLOCK_INITIALIZER; \
    Xr_; })

#define rbuf_drop(rbuf) __extension__({ \
    __auto_type Xr_ = rbuf; \
    assert(pthread_rwlock_destroy(&Xr_->lock) == 0); \
    free(Xr_); \
    NULL; })

#define rbuf_push(rbuf, elt) __extension__({ \
    __auto_type Xr_ = rbuf; \
    pthread_rwlock_wrlock(&Xr_->lock); \
    Xr_->buf[Xr_->write++ & (Xr_->cap - 1)] = elt; \
    ssize_t Xdiff_ = Xr_->write - LOAD_RLX_(&Xr_->read) - Xr_->cap; \
    if (Xdiff_ > 0) { \
        atomic_fetch_add_explicit(&Xr_->read, 1, memory_order_relaxed); \
    } \
    pthread_rwlock_unlock(&Xr_->lock); \
    Xdiff_ < 0; })

#define rbuf_trypush(rbuf, elt) __extension__({ \
    __auto_type Xr_ = rbuf; \
    pthread_rwlock_wrlock(&Xr_->lock); \
    bool Xnot_full_ = Xr_->write - LOAD_RLX_(&Xr_->read) < Xr_->cap; \
    if (Xnot_full_) { \
        Xr_->buf[Xr_->write++ & (Xr_->cap - 1)] = elt; \
    } \
    pthread_rwlock_unlock(&Xr_->lock); \
    Xnot_full_; })

#define rbuf_shift(rbuf, elt) __extension__({ \
    __auto_type Xr_ = rbuf; \
    pthread_rwlock_rdlock(&Xr_->lock); \
    bool Xnot_empty_; \
    for ( ; ; ) { \
        size_t Xi_ = atomic_load_explicit(&Xr_->read, memory_order_acquire); \
        if ((Xnot_empty_ = Xi_ != Xr_->write)) { \
            if (atomic_compare_exchange_weak(&Xr_->read, &Xi_, Xi_ + 1)) { \
                elt = Xr_->buf[Xi_ & (Xr_->cap - 1)]; \
                break; \
            } \
        } else { \
            break; \
        } \
    } \
    pthread_rwlock_unlock(&Xr_->lock); \
    Xnot_empty_; })

#define rbuf_peek(rbuf, elt) __extension__({ \
    __auto_type Xr_ = rbuf; \
    pthread_rwlock_rdlock(&Xr_->lock); \
    size_t Xi_ = atomic_load_explicit(&Xr_->read, memory_order_acquire); \
    bool Xnot_empty_ = Xi_ != Xr_->write; \
    if (Xnot_empty_) { \
        elt = Xr_->buf[Xi_ & (Xr_->cap - 1)]; \
    } \
    pthread_rwlock_unlock(&Xr_->lock); \
    Xnot_empty_; })

#endif
