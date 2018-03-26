#include <stdio.h>
#include <pthread.h>
#include "ringbuf.h"

#define THREADC 16

RINGBUF_DEF(int);
RINGBUF_DEF_PTR(int);

void *
adder(void *arg) {
    ringbuf(int) *rbuf = (ringbuf(int) *)arg;
    int i;
    unsigned long long sum = 0;
    for ( ; ; ) {
        /* clang warns about this line; seems like it could be a bug */
        while (!RINGBUF_POP(rbuf, i));
        if (i == -1) {
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
    int i;
    ringbuf(int) *rbuf = RINGBUF_MAKE(int, 4);
    assert(RINGBUF_PUSH(rbuf, 0));
    assert(RINGBUF_PEEK(rbuf, i));
    assert(i == 0);
    assert(RINGBUF_PUSH(rbuf, -1));
    assert(RINGBUF_POP(rbuf, i));
    assert(RINGBUF_PUSH(rbuf, -2));
    assert(RINGBUF_PUSH(rbuf, -3));
    assert(!RINGBUF_PUSH(rbuf, -4));
    assert(!RINGBUF_PUSH(rbuf, -5));
    assert(!RINGBUF_PUSH(rbuf, -6));
    assert(RINGBUF_POP(rbuf, i));
    assert(i == -3);
    assert(RINGBUF_POP(rbuf, i));
    assert(i == -4);
    assert(RINGBUF_POP(rbuf, i));
    assert(i == -5);
    assert(RINGBUF_POP(rbuf, i));
    assert(i == -6);
    assert(!RINGBUF_POP(rbuf, i));
    rbuf = RINGBUF_DROP(rbuf);

    rbuf = RINGBUF_MAKE(int, 512);
    for (int i = 0; RINGBUF_PUSH(rbuf, i); i--);
    int j;
    for (int i = 0; RINGBUF_POP(rbuf, j); i--) {
        assert(i == j);
    }
    rbuf = RINGBUF_DROP(rbuf);

    /* same deal as slice.h unfortunately */
    ringbuf(PTR_OF(int)) *rbufp = RINGBUF_MAKE(PTR_OF(int), 1);
    assert(!RINGBUF_PUSH(rbufp, &j));
    int *k;
    assert(RINGBUF_POP(rbufp, k));
    assert(&j == k);
    rbufp = RINGBUF_DROP(rbufp);

    pthread_t pool[THREADC];
    rbuf = RINGBUF_MAKE(int, 2);
    for (unsigned i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, rbuf) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        while (!RINGBUF_TRYPUSH(rbuf, i));
    }
    for (unsigned i = 0; i < THREADC; i++) {
        while (!RINGBUF_TRYPUSH(rbuf, -1));
    }
    unsigned long long sum = 0;
    unsigned long long *r;
    for (unsigned i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ull * 100001ull)/2));

    printf("All tests passed\n");
}
