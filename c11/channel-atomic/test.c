#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if ATOMIC_LLONG_LOCK_FREE != 2
#error "probably doesn't work if _Atomic uint64_ts are not lock-free"
#elif !defined(__BYTE_ORDER__) || (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error "could be trivially modified for big endian but i don't have a box"
#endif

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
#define ch_sel_send(c, type, elt) channel_select_send(c, sizeof(type), &elt)
#define ch_sel_recv(c, type, elt) channel_select_recv(c, sizeof(type), &elt)

#define CH_OK 0x0
#define CH_CLOSED 0x1
#define CH_FULL 0x2
#define CH_EMPTY 0x4
#define CH_BUSY 0x8
#define CH_WBLOCK 0x10
#define CH_TIMEDOUT 0x20

#define CH_OP_SEND_ 0x1
#define CH_OP_RECV_ 0x2

#define store_rlx_(obj, des) \
    atomic_store_explicit(obj, des, memory_order_relaxed)
#define store_rel_(obj, des) \
    atomic_store_explicit(obj, des, memory_order_release)
#define load_acq_(obj) atomic_load_explicit(obj, memory_order_acquire)
#define load_rlx_(obj) atomic_load_explicit(obj, memory_order_relaxed)
#define cas_weak_(obj, exp, des) atomic_compare_exchange_weak(obj, exp, des)

typedef union aun64 {
    _Atomic uint64_t u64;
    struct {
        _Atomic uint16_t a;
        _Atomic uint16_t b;
        _Atomic uint16_t c;
        _Atomic uint16_t d;
    } u16;
} aun64;

typedef union un64 {
    uint64_t u64;
    struct {
        uint16_t a;
        uint16_t b;
        uint16_t c;
        uint16_t d;
    } u16;
} un64;

typedef struct channel_buf_ {
    uint16_t cap;
    _Atomic uint16_t refc;
    aun64 wwrc; /* write, written read, closed */
    /* sem */
    char buf[];
} channel_buf_;

typedef struct channel_unbuf_ {
    uint16_t cap;
} channel_unbuf_;

typedef union channel {
    struct header {
        uint16_t cap;
        _Atomic uint16_t refc;
    } hdr;
    channel_buf_ buf;
    channel_unbuf_ unbuf;
} channel;

channel *
channel_make(size_t eltsize, size_t cap) {
    channel *c;
    if (cap > 0) {
        c = calloc(1, offsetof(channel_buf_, buf) + (cap * eltsize));
    } else {
        c = malloc(1);
        assert(false);
    }
    c->hdr.cap = cap;
    store_rlx_(&c->buf.refc, 1);
    return c;
}

void
channel_close(channel *c) {
    if (c->hdr.cap > 0) {
        if (atomic_fetch_sub(&c->buf.refc, 1) == 1) {
            store_rel_(&c->buf.wwrc.u16.a, true);
        }
    }
}

int
channel_trysend_buf_(channel_buf_ *c, size_t eltsize, void *elt) {
    uint16_t written, write;
top:
    do {
        un64 wwrc = {load_acq_(&c->wwrc.u64)};
        if (wwrc.u16.a) {
            return CH_CLOSED;
        }
        uint16_t read = wwrc.u16.b;
        written = wwrc.u16.c, write = wwrc.u16.d;
        if (write - read == c->cap) {
            /* return CH_FULL; */
            sched_yield();
            goto top;
        }
    } while (!cas_weak_(&c->wwrc.u16.d, &write, write + 1));

    memcpy(c->buf + ((write & (c->cap -1)) * eltsize), elt, eltsize);
    while (!cas_weak_(&c->wwrc.u16.c, &written, written + 1)) {
        sched_yield();
    }
    return CH_OK;
}

int
channel_tryrecv_buf_(channel_buf_ *c, size_t eltsize, void *elt) {
    uint16_t read;
top:
    do {
        un64 wwrc = {load_acq_(&c->wwrc.u64)};
        read = wwrc.u16.b;
        uint16_t written = wwrc.u16.c;
        if (read == written) {
            if (wwrc.u16.a) {
                return CH_CLOSED;
            }
            /*
            uint16_t write = wwrc.u16.d;
            if (written < write) {
                sched_yield();
                continue;
            }
            */
            /* return CH_EMPTY; */
            sched_yield();
            goto top;
        }
        memcpy(elt, c->buf + ((read & (c->cap -1)) * eltsize), eltsize);
    } while (!cas_weak_(&c->wwrc.u16.b, &read, read + 1));
    return CH_OK;
}

int
channel_send_buf_(channel_buf_ *c, size_t eltsize, void *elt) {
    int rc = channel_trysend_buf_(c, eltsize, elt);
    if (rc == CH_FULL) {
    }
    return rc;
}

int
channel_send(channel *c, size_t eltsize, void *elt) {
    if (c->hdr.cap > 0) {
        return channel_send_buf_(&c->buf, eltsize, elt);
    }
    return 0;
}

int
channel_recv_buf_(channel_buf_ *c, size_t eltsize, void *elt) {
    int rc = channel_tryrecv_buf_(c, eltsize, elt);
    if (rc == CH_FULL) {
    }
    return rc;
}

int
channel_recv(channel *c, size_t eltsize, void *elt) {
    if (c->hdr.cap > 0) {
        return channel_recv_buf_(&c->buf, eltsize, elt);
    }
    return 0;
}

void *
bench(void *arg) {
    channel *chan = (channel *)arg;
    int i;
    while (ch_recv(chan, int, i) != CH_CLOSED) {
    }
    printf("done\n");
    return NULL;
}

#define THREADC 1

int
main() {
    channel *chan = ch_make(int, 1);
    pthread_t pool[THREADC];
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, bench, chan) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        ch_send(chan, int, i);
    }
    ch_close(chan);
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], NULL) == 0);
    }
    // ch_drop(chan);
}
