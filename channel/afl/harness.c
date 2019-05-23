#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "../channel.h"

CHANNEL_EXTERN_DECL;

#define THREADC 16

_Atomic unsigned totals[256];
_Atomic bool closed;

void *
receiver(void *arg) {
    channel *c = (channel *)arg;
    uint8_t i;
    while (ch_recv(c, &i) == CH_OK) {
        atomic_fetch_add_explicit(totals + i, 1, memory_order_relaxed);
    }
    assert(atomic_load_explicit(&closed, memory_order_acquire));
    return NULL;
}

void *
sender(void *arg) {
    channel *cc = (channel *)arg;
    channel *c, *c1;
    assert(ch_recv(cc, &c) == CH_OK);
    assert(ch_recv(cc, &c1) == CH_OK);
    uint8_t i;
    while (ch_recv(c, &i) == CH_OK) {
        assert(ch_timedsend(c1, &i, 100) != CH_CLOSED);
    }
    assert(atomic_load_explicit(&closed, memory_order_acquire));
    return NULL;
}

int
main(void) {
    srand(time(NULL));
    char *buf = NULL;
    size_t bufcap = 0;
    int sender_cap = getline(&buf, &bufcap, stdin) % 2;
    int receiver_cap = getline(&buf, &bufcap, stdin) % 5;

    pthread_t sender_tpool[THREADC];
    pthread_t receiver_tpool[THREADC];
    channel *cc = ch_make(channel *, 0);
    channel *sender_cpool[THREADC];
    uint8_t i8;
    channel_case cases[THREADC];
    channel *receiver_cpool[THREADC];
    for (size_t i = 0; i < THREADC; i++) {
        assert(pthread_create(sender_tpool + i, NULL, sender, cc) == 0);
        sender_cpool[i] = ch_make(uint8_t, sender_cap);
        assert(ch_send(cc, &sender_cpool[i]) == CH_OK);
        cases[i] = (const channel_case){
            .c = sender_cpool[i], .msg = &i8, .op = CH_SEND
        };
        receiver_cpool[i] = ch_make(uint8_t, receiver_cap);
        assert(ch_send(cc, &receiver_cpool[i]) == CH_OK);
        assert(pthread_create(
            receiver_tpool + i, NULL, receiver, receiver_cpool[i]) == 0);
    }
    ch_close(cc);
    cc = ch_drop(cc);

    for ( ; ; ) {
        ssize_t readlen = getline(&buf, &bufcap, stdin);
        if (readlen < 0) {
            break;
        }
        for (ssize_t i = 0; i < readlen; i++) {
            i8 = buf[i];
            ch_timedalt(cases, THREADC, ((uint64_t)i8) * 100);
        }
    }
    free(buf);
    buf = NULL;
    bufcap = 0;

    atomic_store_explicit(&closed, true, memory_order_release);
    for (size_t i = 0; i < THREADC; i++) {
        ch_close(sender_cpool[i]);
        assert(pthread_join(sender_tpool[i], NULL) == 0);
        sender_cpool[i] = ch_drop(sender_cpool[i]);
        ch_close(receiver_cpool[i]);
        assert(pthread_join(receiver_tpool[i], NULL) == 0);
        receiver_cpool[i] = ch_drop(receiver_cpool[i]);
    }

    for (size_t i = 0; i < 256; i++) {
        printf("%u ", totals[i]);
    }
    printf("\b\n");
    return 0;
}
