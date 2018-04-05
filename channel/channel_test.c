#include <stdio.h>
#include <pthread.h>
#include "channel.h"

#define THREADC 16

CHANNEL_DEF(int);
CHANNEL_DEF_PTR(int);

void *
adder(void *arg) {
    channel(int) *chan = (channel(int) *)arg;
    int i;
    unsigned long long sum = 0;
    for ( ; ; ) {
        if (CHANNEL_POP(chan, i) == CHANNEL_CLOSED) {
            break;
        }
        sum += i;
    }
    unsigned long long *ret = malloc(sizeof(*ret));
    *ret = sum;
    printf("%llu\n", sum);
    return ret;
}

int
main() {
    channel(int) *chan = CHANNEL_MAKE(int, 2);
    pthread_t pool[THREADC];
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, chan) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        CHANNEL_PUSH(chan, i);
    }
    CHANNEL_CLOSE(chan);
    unsigned long long sum = 0;
    unsigned long long *r;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ull * 100001ull)/2));
    CHANNEL_DROP(chan);

    printf("lmao\n");

    chan = CHANNEL_MAKE(int, 0);
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, chan) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        CHANNEL_PUSH(chan, i);
    }
    CHANNEL_CLOSE(chan);
    sum = 0;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ull * 100001ull)/2));
    CHANNEL_DROP(chan);

    printf("All tests passed\n");
}
