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
#include <time.h>
#include <unistd.h>

#define CH_OK 0x0
#define CH_FULL 0x1
#define CH_EMPTY 0x2
#define CH_BUSY 0x4
#define CH_WBLOCK 0x8
#define CH_TIMEDOUT 0x10
#define CH_CLOSED 0x20

#ifndef MINIMAL_
#define MINIMAL_
#define MACRO_CONCAT_(x, y) x##y
#define MC_(x, y) MACRO_CONCAT_(x, y)
#define PTR_OF(T) MC_(T, ptr)
#endif

#define channel(T) MC_(channel_, T)

#define CHANNEL_DEF(T) \
    typedef struct channel(T) { \
        size_t cap, write, read; \
        pthread_mutex_t lock, wlock, rlock; \
        pthread_cond_t empty, full; \
        uint16_t refs; \
        bool closed; \
        T buf[]; \
    } channel(T)
#define CHANNEL_DEF_PTR(T) \
    typedef T *PTR_OF(T); \
    CHANNEL_DEF(PTR_OF(T))

#define ch_make(T, cap_) __extension__({ \
    __auto_type Xcap_ = cap_; \
    channel(T) *Xc_ = calloc( \
        1, offsetof(channel(T), buf) + ((Xcap_ ? : 1) * sizeof(T))); \
    assert((Xcap_ & (Xcap_ - 1)) == 0 && Xc_); \
    Xc_->cap = Xcap_; \
    Xc_->lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER; \
    Xc_->wlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER; \
    Xc_->rlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER; \
    Xc_->empty = (pthread_cond_t)PTHREAD_COND_INITIALIZER; \
    Xc_->full = (pthread_cond_t)PTHREAD_COND_INITIALIZER; \
    Xc_->refs = 1; \
    Xc_; })

#define ch_dup(chan) __extension__({ \
    __extension__ __auto_type Xc_ = chan; \
    pthread_mutex_lock(&Xc_->lock); \
    Xc_->refs++; \
    pthread_mutex_unlock(&Xc_->lock); \
    Xc_; })

#define ch_close(chan) do { \
    __extension__ __auto_type Xc_ = chan; \
    pthread_mutex_lock(&Xc_->lock); \
    assert(Xc_->refs > 0); \
    if (--Xc_->refs == 0) { \
        Xc_->closed = true; \
        pthread_cond_broadcast(&Xc_->empty); \
        pthread_cond_broadcast(&Xc_->full); \
    } \
    pthread_mutex_unlock(&Xc_->lock); } while (0)

#define ch_drop(chan) __extension__({ \
    __auto_type Xc_ = chan; \
    assert(pthread_mutex_destroy(&Xc_->lock) == 0 && \
               pthread_mutex_destroy(&Xc_->wlock) == 0 && \
               pthread_mutex_destroy(&Xc_->rlock) == 0 && \
               pthread_cond_destroy(&Xc_->empty) == 0 && \
               pthread_cond_destroy(&Xc_->full) == 0); \
    free(Xc_); \
    NULL; })

#define ch_send(chan, elt_) __extension__({ \
    __label__ cleanup; \
    __auto_type Xc_ = chan; \
    int Xret_ = CH_OK; \
    pthread_mutex_lock(&Xc_->wlock); \
    pthread_mutex_lock(&Xc_->lock); \
    if (Xc_->cap > 0) { \
        while (Xc_->write - Xc_->read == Xc_->cap) { \
            if (Xc_->closed) { \
                Xret_ = CH_CLOSED; \
                goto cleanup; \
            } \
            pthread_cond_wait(&Xc_->full, &Xc_->lock); \
        } \
        if (Xc_->closed) { \
            Xret_ = CH_CLOSED; \
            goto cleanup; \
        } \
        Xc_->buf[Xc_->write++ & (Xc_->cap - 1)] = elt_; \
        pthread_cond_signal(&Xc_->empty); \
    } else { \
        if (Xc_->closed) { \
            Xret_ = CH_CLOSED; \
            goto cleanup; \
        } \
        *Xc_->buf = elt_; \
        Xc_->write = 1; \
        pthread_cond_signal(&Xc_->empty); \
        pthread_cond_wait(&Xc_->full, &Xc_->lock); \
        Xc_->read = 0; \
    } \
cleanup: \
    pthread_mutex_unlock(&Xc_->lock); \
    pthread_mutex_unlock(&Xc_->wlock); \
    Xret_; })

#define ch_trysend(chan, elt_) __extension__({ \
    __label__ cleanup; \
    __label__ cleanup1; \
    __auto_type Xc_ = chan; \
    int Xret_ = CH_OK; \
    if (Xc_->cap > 0) { \
        pthread_mutex_lock(&Xc_->wlock); \
        pthread_mutex_lock(&Xc_->lock); \
        if (Xc_->closed) { \
            Xret_ = CH_CLOSED; \
            goto cleanup; \
        } \
        if (Xc_->write - Xc_->read == Xc_->cap) { \
            Xret_ = CH_FULL; \
            goto cleanup; \
        } \
        Xc_->buf[Xc_->write++ & (Xc_->cap - 1)] = elt_; \
        pthread_cond_signal(&Xc_->empty); \
    } else { \
        if (pthread_mutex_trylock(&Xc_->wlock) == EBUSY) { \
            Xret_ = CH_BUSY; \
            goto cleanup1; \
        } \
        pthread_mutex_lock(&Xc_->lock); \
        if (Xc_->closed) { \
            Xret_ = CH_CLOSED; \
            goto cleanup; \
        } \
        if (Xc_->read == 0) { \
            Xret_ = CH_WBLOCK; \
            goto cleanup; \
        } \
        *Xc_->buf = elt_; \
        Xc_->write = 1; \
        pthread_cond_signal(&Xc_->empty); \
        pthread_cond_wait(&Xc_->full, &Xc_->lock); \
        Xc_->read = 0; \
    } \
cleanup: \
    pthread_mutex_unlock(&Xc_->lock); \
    pthread_mutex_unlock(&Xc_->wlock); \
cleanup1: \
    Xret_; })

#if _POSIX_TIMEOUTS >= 200112L
#define ch_timedsend(chan, offset, elt_) __extension__({ \
    __label__ cleanup; \
    __label__ cleanup1; \
    __label__ cleanup2; \
    struct timespec Xts_ = channel_add_offset_(offset); \
    __auto_type Xc_ = chan; \
    int Xret_ = CH_OK; \
    if (pthread_mutex_timedlock(&Xc_->wlock, &Xts_) == ETIMEDOUT) { \
        Xret_ = CH_BUSY | CH_TIMEDOUT; \
        goto cleanup2; \
    } \
    if (pthread_mutex_timedlock(&Xc_->lock, &Xts_) == ETIMEDOUT) { \
        Xret_ = CH_BUSY | CH_TIMEDOUT; \
        goto cleanup1; \
    } \
    if (Xc_->cap > 0) { \
        while (Xc_->write - Xc_->read == Xc_->cap) { \
            if (Xc_->closed) { \
                Xret_ = CH_CLOSED; \
                goto cleanup; \
            } \
            if (pthread_cond_timedwait(&Xc_->full, &Xc_->lock, &Xts_) == \
                    ETIMEDOUT) { \
                Xret_ = (Xc->closed ? CH_CLOSED : CH_FULL) | CH_TIMEDOUT; \
                goto cleanup; \
            } \
        } \
        if (Xc_->closed) { \
            Xret_ = CH_CLOSED; \
            goto cleanup; \
        } \
        Xc_->buf[Xc_->write++ & (Xc_->cap - 1)] = elt_; \
        pthread_cond_signal(&Xc_->empty); \
    } else { \
        if (Xc_->closed) { \
            Xret_ = CH_CLOSED; \
            goto cleanup; \
        } \
        *Xc_->buf = elt_; \
        Xc_->write = 1; \
        pthread_cond_signal(&Xc_->empty); \
        if (pthread_cond_timedwait(&Xc_->full, &Xc_->lock, &Xts_) == \
                ETIMEDOUT) { \
            Xret_ = (Xc->closed ? CH_CLOSED : CH_FULL) | CH_TIMEDOUT; \
            Xc_->write = 0; \
            goto cleanup; \
        } \
    } \
cleanup: \
    pthread_mutex_unlock(&Xc_->lock); \
cleanup1: \
    pthread_mutex_unlock(&Xc_->wlock); \
cleanup2: \
    Xret_; })
#else
#define ch_timedsend(chan, offset, elt_) \
    _Pragma ("GCC error \"pthread_mutex_timedlock is not implemented \"")
#endif

#define ch_forcesend(chan, elt_) __extension__({ \
    __auto_type Xc_ = chan; \
    int Xret_ = CH_OK; \
    assert(Xc_->cap > 0); \
    pthread_mutex_lock(&Xc_->wlock); \
    pthread_mutex_lock(&Xc_->lock); \
    if (Xc_->closed) { \
        Xret_ = CH_CLOSED; \
    } else { \
        Xc_->buf[Xc_->write++ & (Xc_->cap - 1)] = elt_; \
        if (Xc_->write - Xc_->read > Xc_->cap) { \
            Xret_ = CH_FULL; \
            Xc_->read++; \
        } \
        pthread_cond_signal(&Xc_->empty); \
    } \
    pthread_mutex_unlock(&Xc_->lock); \
    pthread_mutex_unlock(&Xc_->wlock); \
    Xret_; })

#define ch_recv(chan, elt_) __extension__({ \
    __label__ cleanup; \
    __auto_type Xc_ = chan; \
    int Xret_ = CH_OK; \
    pthread_mutex_lock(&Xc_->rlock); \
    pthread_mutex_lock(&Xc_->lock); \
    if (Xc_->cap > 0) { \
        while (Xc_->read == Xc_->write) { \
            if (Xc_->closed) { \
                Xret_ = CH_CLOSED; \
                goto cleanup; \
            } \
            pthread_cond_wait(&Xc_->empty, &Xc_->lock); \
        } \
        elt_ = Xc_->buf[Xc_->read++ & (Xc_->cap - 1)]; \
        pthread_cond_signal(&Xc_->full); \
    } else { \
        Xc_->read = 1; \
        while (Xc_->write == 0) { \
            if (Xc_->closed) { \
                Xret_ = CH_CLOSED; \
                goto cleanup; \
            } \
            pthread_cond_wait(&Xc_->empty, &Xc_->lock); \
        } \
        elt_ = *Xc_->buf; \
        Xc_->write = Xc_->read = 0; \
        pthread_cond_signal(&Xc_->full); \
    } \
cleanup: \
    pthread_mutex_unlock(&Xc_->lock); \
    pthread_mutex_unlock(&Xc_->rlock); \
    Xret_; })

#define ch_tryrecv(chan, elt_) __extension__({ \
    __label__ cleanup; \
    __label__ cleanup1; \
    __auto_type Xc_ = chan; \
    int Xret_ = CH_OK; \
    if (Xc_->cap > 0) { \
        pthread_mutex_lock(&Xc_->rlock); \
        pthread_mutex_lock(&Xc_->lock); \
        if (Xc_->closed) { \
            Xret_ = CH_CLOSED; \
            goto cleanup; \
        } \
        if (Xc_->read == Xc_->write) { \
            Xret_ = CH_EMPTY; \
            goto cleanup; \
        } \
        elt_ = Xc_->buf[Xc_->read++ & (Xc_->cap - 1)]; \
        pthread_cond_signal(&Xc_->full); \
    } else { \
        if (pthread_mutex_trylock(&Xc_->rlock) == EBUSY) { \
            Xret_ = CH_BUSY; \
            goto cleanup1; \
        } \
        pthread_mutex_lock(&Xc_->lock); \
        if (Xc_->closed) { \
            Xret_ = CH_CLOSED; \
            goto cleanup; \
        } \
        if (Xc_->write == 0) { \
            Xret_ = CH_WBLOCK; \
            goto cleanup; \
        } \
        elt_ = *Xc_->buf; \
        Xc_->write = 0; \
        pthread_cond_signal(&Xc_->full); \
    } \
cleanup: \
    pthread_mutex_unlock(&Xc_->lock); \
    pthread_mutex_unlock(&Xc_->rlock); \
cleanup1: \
    Xret_; })

#if _POSIX_TIMEOUTS >= 200112L
#define ch_timedrecv(chan, offset, elt_) __extension__({ \
    __label__ cleanup; \
    __label__ cleanup1; \
    __label__ cleanup2; \
    struct timespec Xts_ = channel_add_offset_(offset); \
    __auto_type Xc_ = chan; \
    int Xret_ = CH_OK; \
    if (pthread_mutex_timedlock(&Xc_->rlock, &Xts_) == ETIMEDOUT) { \
        Xret_ = CH_BUSY | CH_TIMEDOUT; \
        goto cleanup2; \
    } \
    if (pthread_mutex_timedlock(&Xc_->lock, &Xts_) == ETIMEDOUT) { \
        Xret_ = CH_BUSY | CH_TIMEDOUT; \
        goto cleanup1; \
    } \
    if (Xc_->cap > 0) { \
        while (Xc_->read == Xc_->write) { \
            if (Xc_->closed) { \
                Xret_ = CH_CLOSED; \
                goto cleanup; \
            } \
            if (pthread_cond_timedwait(&Xc_->empty, &Xc_->lock, &Xts_) == \
                    ETIMEDOUT) { \
                Xret_ = (Xc_->closed ? CH_CLOSED : CH_EMPTY) | CH_TIMEDOUT; \
                goto cleanup; \
            } \
        } \
        elt_ = Xc_->buf[Xc_->read++ & (Xc_->cap - 1)]; \
        pthread_cond_signal(&Xc_->full); \
    } else { \
        while (Xc_->write == 0) { \
            if (Xc_->closed) { \
                Xret_ = CH_CLOSED; \
                goto cleanup; \
            } \
            if (pthread_cond_timedwait(&Xc_->empty, &Xc_->lock, &Xts_) == \
                    ETIMEDOUT) { \
                Xret_ = (Xc_->closed ? CH_CLOSED : CH_EMPTY) | CH_TIMEDOUT; \
                goto cleanup; \
            } \
        } \
        elt_ = *Xc_->buf; \
        Xc_->write = 0; \
        pthread_cond_signal(&Xc_->full); \
    } \
cleanup: \
    pthread_mutex_unlock(&Xc_->lock); \
cleanup1: \
    pthread_mutex_unlock(&Xc_->rlock); \
cleanup2: \
    Xret_; })
#else
#define ch_timedrecv(chan, offset ,elt_) \
    _Pragma ("GCC error \"pthread_mutex_timedlock is not implemented \"")
#endif

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

static inline struct timespec
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

#endif
