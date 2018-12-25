/* Shamelessly copied from babby's first Go program. Yes, it's slower than
 * trial division, yes it leaks, kind of. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../channel.h"

CHANNEL_EXTERN_DECL;

#define THREADC 512

void *
generate(void *arg) {
    channel *chan = (channel *)arg;
    for (int i = 2; ch_send(chan, i) == CH_OK; i++);
    return NULL;
}

void *
filter(void *args) {
    channel *chan_in = ((channel **)args)[0];
    channel *chan_out = ((channel **)args)[1];
    int p = (intptr_t)((void **)args)[2];
    free(args);

    int i;
    while(ch_recv(chan_in, i) == CH_OK) {
        if (i % p != 0) {
            ch_send(chan_out, i);
        }
    }
    return NULL;
}

int
main(void) {
    channel *chan = ch_make(int, 0), *chan1;
    pthread_t gen, fltrpool[THREADC];
    pthread_create(&gen, NULL, generate, chan);

    int p = 0;
    for (int i = 0; i < THREADC; i++) {
        ch_recv(chan, p);
        printf("%d\n", p);

        void **args = malloc(3 * sizeof(*args));
        args[0] = chan;
        args[1] = (chan1 = ch_make(int, 0));
        args[2] = (void *)(intptr_t)p;

        pthread_create(fltrpool + i, NULL, filter, args);
        chan = chan1;
    }
    return 0;
}
