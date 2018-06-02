#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include "../channel.h"

#define THREADC 16
#define LIM 100000ll

CHANNEL_EXTERN_DECL;

void *
sender(void *arg) {
    channel *chan = (channel *)arg;
    for (int i = 1; i <= LIM; i++) {
        ch_send(chan, i);
    }
    ch_close(chan);
    return NULL;
}

void *
receiver(void *arg) {
    channel *chan = (channel *)arg;
    int i;
    long long sum = 0;
    while (ch_recv(chan, i) != CH_CLOSED) {
        sum += i;
    }
    printf("%lld\n", sum);
    return (void *)sum;
}

int
main(void) {
    channel *chan = ch_make(int, 8);
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
    return 0;
}
