/* Shamelessly copied from babby's first Go program. Yes, it's slower than
 * trial division, yes it leaks, kind of. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../channel/channel.h"
#include "../slice_gnu99/slice.h"

SLICE_DEF(int);
CHANNEL_DEF(int);

#define THREADC 128

void *
generate(void *arg) {
    channel(int) *chan = (channel(int) *)arg;
    for (int i = 2; ch_send(chan, i) == CH_OK; i++);
    return NULL;
}

void *
filter(void *args) {
    channel(int) *chan_in = ((channel(int) **)args)[0];
    channel(int) *chan_out = ((channel(int) **)args)[1];
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
main() {
    slice(int) *primes = s_make(int, 0, 8);
    channel(int) *chan = ch_make(int, 0), *chan1;
    pthread_t gen, fltrpool[THREADC];
    pthread_create(&gen, NULL, generate, chan);

    int p = 0;
    for (int i = 0; i < THREADC; i++) {
        ch_recv(chan, p);
        s_append(primes, p);

        void **args = malloc(3 * sizeof(*args));
        args[0] = chan;
        args[1] = (chan1 = ch_make(int, 0));
        args[2] = (void *)(intptr_t)p;

        pthread_create(fltrpool + i, NULL, filter, args);
        chan = chan1;
    }

    for (unsigned i = 0; i < primes->len; i++) {
        printf("%d\n", s_arr(primes)[i]);
    }
    s_drop(primes);
    return 0;
}
