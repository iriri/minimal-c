/* XXX: WORK IN PROGRESS
 *
 * Semantics inspired by channels from the Go programming language.
 * Implementation of buffered send/recv inspired by "Go channels on steroids"
 * by Dmitry Vyukov. */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
#error "not sure if this works if _Atomic uint64_ts aren't lock-free"
#elif !defined(__BYTE_ORDER__) || (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error "needs trivial changes for big endian but i don't have a box to test on"
#endif

#define ch_make(type, cap) channel_make(sizeof(uint32_t) + sizeof(type), cap)
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
#define ch_sel_send(c, type, elt) channel_select_send(c, sizeof(type), &elt)
#define ch_sel_recv(c, type, elt) channel_select_recv(c, sizeof(type), &elt)

#define CH_OK 0x0
#define CH_CLOSED 0x1
#define CH_FULL 0x2
#define CH_EMPTY 0x4
#define CH_BUSY 0x8
#define CH_WBLOCK 0x10
#define CH_TIMEDOUT 0x20

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

typedef union aun64 {
    _Atomic uint64_t u64;
    struct {
        _Atomic uint32_t index;
        _Atomic uint32_t lap;
    } u32;
} aun64;

typedef union un64 {
    uint64_t u64;
    struct {
        uint32_t index;
        uint32_t lap;
    } u32;
} un64;

typedef struct channel_waiter_ {
    struct channel_waiter_ *prev, *next;
#if _POSIX_SEMAPHORES >= 200112L
    sem_t sem;
#elif defined __APPLE__
    dispatch_semaphore_t sem;
#endif
} channel_waiter_;

typedef struct channel_buf_ {
    uint32_t cap;
    _Atomic uint32_t refc;
    channel_waiter_ *_Atomic sendq, *_Atomic recvq;
    pthread_mutex_t lock;
    aun64 write, read;
    char buf[];
} channel_buf_;

typedef struct channel_unbuf_ {
    uint32_t cap;
    _Atomic uint32_t refc;
    channel_waiter_ *_Atomic sendq, *_Atomic recvq;
    pthread_mutex_t lock;
    char buf[];
} channel_unbuf_;

typedef union channel {
    struct header {
        uint32_t cap;
        _Atomic uint32_t refc;
        channel_waiter_ *_Atomic sendq, *_Atomic recvq;
        pthread_mutex_t lock;
    } hdr;
    channel_buf_ buf;
    channel_unbuf_ unbuf;
} channel;

/* If the capacity is 0 then the channel is unbuffered. Otherwise the channel
 * is buffered and the capacity must be a power of 2 */
channel *
channel_make(size_t boxsize, size_t cap) {
    channel *c;
    if (cap > 0) {
        assert((c = calloc(1, offsetof(channel_buf_, buf) + (cap * boxsize))));
        store_rlx_(&c->buf.read.u32.lap, 1);
    } else {
        assert(false);
    }
    c->hdr.cap = cap;
    store_rlx_(&c->hdr.refc, 1);
    c->hdr.lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    return c;
}

/* Increases the reference count of the channel and returns it. Intended to be
 * used with multiple producers that independently close the channel */
channel *
channel_dup(channel *c) {
    assert(faa_acq_rel_(&c->hdr.refc, 1) != 0);
    return c;
}

/* Deallocates all resources associated with the channel */
channel *
channel_drop(channel *c) {
    assert(pthread_mutex_destroy(&c->hdr.lock) == 0);
    free(c);
    return NULL;
}

void
channel_wait_(
    channel_waiter_ *_Atomic *waitq,
    channel_waiter_ *_Atomic *otherq,
    pthread_mutex_t *lock
) {
    channel_waiter_ *o;
    if ((o = load_acq_(otherq))) {
        if (o->next) {
            o->next->prev = o->prev;
        }
        store_rel_(otherq, o->next);
        pthread_mutex_unlock(lock);
#if _POSIX_SEMAPHORES >= 200112L
        sem_post(&o->sem);
#elif defined __APPLE__
        dispatch_semaphore_signal(o->sem);
#endif
        return;
    }

    channel_waiter_ *w = malloc(sizeof(*w));
    w->next = NULL;
    if (!load_rlx_(waitq)) {
        store_rel_(waitq, w);
        w->prev = w;
    } else {
        channel_waiter_ *wq = load_rlx_(waitq);
        w->prev = wq->prev;
        wq->prev = w;
        w->prev->next = w;
    }
#if _POSIX_SEMAPHORES >= 200112L
    sem_init(&w->sem, 0, 0);
    pthread_mutex_unlock(lock);
    sem_wait(&w->sem);
    sem_destroy(&w->sem);
#elif defined __APPLE__
    w->sem = dispatch_semaphore_create(0);
    pthread_mutex_unlock(lock);
    dispatch_semaphore_wait(w->sem, DISPATCH_TIME_FOREVER);
    dispatch_release(w->sem);
#endif
    free(w);
}

void
channel_unblock_waiter_(
    channel_waiter_ *_Atomic *waitq,
    pthread_mutex_t *lock
) {
    if (!load_acq_(waitq)) {
        return;
    }
    if (lock) {
        pthread_mutex_lock(lock);
    }
    channel_waiter_ *w;
    if (!(w = load_rlx_(waitq))) {
        if (lock) {
            pthread_mutex_unlock(lock);
        }
        return;
    }
    if (w->next) {
        w->next->prev = w->prev;
    }
    store_rel_(waitq, w->next);
    if (lock) {
        pthread_mutex_unlock(lock);
    }
#if _POSIX_SEMAPHORES >= 200112L
    sem_post(&w->sem);
#elif defined __APPLE__
    dispatch_semaphore_signal(w->sem);
#endif
}

/* Closes the channel if the caller has the last reference to the channel.
 * Decreases the reference count otherwise */
void
channel_close(channel *c) {
    uint32_t refc = fas_acq_rel_(&c->hdr.refc, 1);
    if (refc == 1) {
        pthread_mutex_lock(&c->buf.lock);
        while (load_acq_(&c->buf.sendq)) {
            channel_unblock_waiter_(&c->buf.sendq, NULL);
        }
        while (load_acq_(&c->buf.recvq)) {
            channel_unblock_waiter_(&c->buf.recvq, NULL);
        }
        pthread_mutex_unlock(&c->buf.lock);
        return;
    }
    assert(refc != 0);
}

int
channel_trysend_buf_(channel_buf_ *c, size_t eltsize, void *elt, bool rcbusy) {
    for ( ; ; ) {
        if (rcbusy && load_acq_(&c->sendq)) {
            return CH_BUSY;
        }
        if (load_acq_(&c->refc) == 0) {
            return CH_CLOSED;
        }
        un64 write = {load_acq_(&c->write.u64)};
        char *box = c->buf + (write.u32.index * (eltsize + sizeof(uint32_t)));
        uint32_t lap = load_acq_((_Atomic uint32_t *)box);
        if (write.u32.lap - lap > 0) {
            sched_yield();
            continue;
            /* return CH_FULL; */
        }

        assert(write.u32.lap == lap);
        uint64_t write1 = write.u32.index + 1 < c->cap ?
                write.u64 + 1 : (uint64_t)(write.u32.lap + 2) << 32;
        if (cas_weak_(&c->write.u64, &write.u64, write1)) {
            memcpy(box + sizeof(uint32_t), elt, eltsize);
            store_rel_((_Atomic uint32_t *)box, lap + 1);
            channel_unblock_waiter_(&c->recvq, &c->lock);
            return CH_OK;
        }
    }
}

int
channel_trysend(channel *c, size_t eltsize, void *elt) {
    if (c->hdr.cap > 0) {
        return channel_trysend_buf_(&c->buf, eltsize, elt, true);
    }
    // return channel_trysend_unbuf(&c->unbuf, eltsize, elt);
    assert(false);
    return 0;
}

int
channel_tryrecv_buf_(channel_buf_ *c, size_t eltsize, void *elt, bool rcbusy) {
    for ( ; ; ) {
        if (rcbusy && load_acq_(&c->sendq)) {
            return CH_BUSY;
        }
        un64 read = {load_acq_(&c->read.u64)};
        char *box = c->buf + (read.u32.index * (eltsize + sizeof(uint32_t)));
        uint32_t lap = load_acq_((_Atomic uint32_t *)box);
        if (read.u32.lap - lap > 0) {
            if (load_acq_(&c->refc) == 0) {
                return CH_CLOSED;
            }
            sched_yield();
            continue;
            /* return CH_EMPTY; */
        }

        assert(read.u32.lap == lap);
        uint64_t read1 = read.u32.index + 1 < c->cap ?
                read.u64 + 1 : (uint64_t)(read.u32.lap + 2) << 32;
        if (cas_weak_(&c->read.u64, &read.u64, read1)) {
            memcpy(elt, box + sizeof(uint32_t), eltsize);
            store_rel_((_Atomic uint32_t *)box, lap + 1);
            channel_unblock_waiter_(&c->sendq, &c->lock);
            return CH_OK;
        }
    }
}

int
channel_tryrecv(channel *c, size_t eltsize, void *elt) {
    if (c->hdr.cap > 0) {
        return channel_tryrecv_buf_(&c->buf, eltsize, elt, true);
    }
    // return channel_tryrecv_unbuf(&c->unbuf, eltsize, elt);
    assert(false);
    return 0;
}

int
channel_send_buf_(channel_buf_ *c, size_t eltsize, void *elt) {
    for (bool rcbusy = true; ; ) {
        int rc = channel_trysend_buf_(c, eltsize, elt, rcbusy);
        if (rc == CH_OK || rc == CH_CLOSED) {
            return rc;
        }

        pthread_mutex_lock(&c->lock);
        if (load_acq_(&c->refc) == 0) {
            pthread_mutex_unlock(&c->lock);
            return CH_CLOSED;
        }
        un64 write = {load_acq_(&c->write.u64)};
        char *elt = c->buf + (write.u32.index * (eltsize + sizeof(uint32_t)));
        if (write.u32.lap == load_acq_((_Atomic uint32_t *)elt)) {
            pthread_mutex_unlock(&c->lock);
            continue;
        }
        channel_wait_(&c->sendq, &c->recvq, &c->lock);
        rcbusy = false;
    }
}

int
channel_send(channel *c, size_t eltsize, void *elt) {
    if (c->hdr.cap > 0) {
        return channel_send_buf_(&c->buf, eltsize, elt);
    }
    // return channel_send_unbuf(&c->unbuf, eltsize, elt);
    assert(false);
    return 0;
}

int
channel_recv_buf_(channel_buf_ *c, size_t eltsize, void *elt) {
    for (bool rcbusy = true; ; ) {
        int rc = channel_tryrecv_buf_(c, eltsize, elt, rcbusy);
        if (rc == CH_OK || rc == CH_CLOSED) {
            return rc;
        }

        pthread_mutex_lock(&c->lock);
        un64 read = {load_acq_(&c->read.u64)};
        char *elt = c->buf + (read.u32.index * (eltsize + sizeof(uint32_t)));
        if (read.u32.lap == load_acq_((_Atomic uint32_t *)elt)) {
            pthread_mutex_unlock(&c->lock);
            // printf("ree %d\n", rc);
            continue;
        }
        if (load_acq_(&c->refc) == 0) {
            pthread_mutex_unlock(&c->lock);
            return CH_CLOSED;
        }
        channel_wait_(&c->recvq, &c->sendq, &c->lock);
        rcbusy = false;
    }
}

int
channel_recv(channel *c, size_t eltsize, void *elt) {
    if (c->hdr.cap > 0) {
        return channel_recv_buf_(&c->buf, eltsize, elt);
    }
    // return channel_recv_unbuf_(&c->buf, eltsize, elt);
    assert(false);
    return 0;
}

#define THREADC 16
#define LIM 100000ll

void *
sender(void *arg) {
    channel *chan = (channel *)arg;
    for (int i = 1; i <= LIM; i++) {
        ch_send(chan, int, i);
    }
    ch_close(chan);
    return NULL;
}

void *
receiver(void *arg) {
    channel *chan = (channel *)arg;
    int i;
    long long sum = 0;
    while (ch_recv(chan, int, i) != CH_CLOSED) {
        sum += i;
    }
    printf("%lld\n", sum);
    return (void *)sum;
}

int
main() {
    channel *chan = ch_make(int, 1);
    pthread_t senders[THREADC];
    pthread_t recvers[THREADC];
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(senders + i, NULL, sender, ch_dup(chan)) == 0);
    }
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(recvers + i, NULL, receiver, chan) == 0);
    }
    ch_close(chan);
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(senders[i], NULL) == 0);
    }
    long long sum = 0, t = 0;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(recvers[i], (void **)&t) == 0);
        sum += t;
    }
    printf("%lld\n", sum);
    assert(sum == ((LIM * (LIM + 1))/2) * THREADC);
    chan = ch_drop(chan);
}
