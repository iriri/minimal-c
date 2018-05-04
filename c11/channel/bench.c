#include <stdio.h>
#include <pthread.h>
#include "channel.h"

CHANNEL_EXTERN_DECL;

#define THREADC 1

void *
bench(void *arg) {
    channel *chan = (channel *)arg;
    int i;
    while (ch_recv(chan, int, i) != CH_CLOSED);
    return NULL;
}

int
main() {
    channel *chan = ch_make(int, 0);
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
    ch_drop(chan);
}
