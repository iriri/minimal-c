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
        /* older versions of clang warn about this line */
        while (!rbuf_shift(rbuf, i));
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
    ringbuf(int) *rbuf = rbuf_make(int, 4);
    assert(rbuf_push(rbuf, 0));
    assert(rbuf_peek(rbuf, i));
    assert(i == 0);
    assert(rbuf_push(rbuf, -1));
    assert(rbuf_shift(rbuf, i));
    assert(rbuf_push(rbuf, -2));
    assert(rbuf_push(rbuf, -3));
    assert(!rbuf_push(rbuf, -4));
    assert(!rbuf_push(rbuf, -5));
    assert(!rbuf_push(rbuf, -6));
    assert(rbuf_shift(rbuf, i));
    assert(i == -3);
    assert(rbuf_shift(rbuf, i));
    assert(i == -4);
    assert(rbuf_shift(rbuf, i));
    assert(i == -5);
    assert(rbuf_shift(rbuf, i));
    assert(i == -6);
    assert(!rbuf_shift(rbuf, i));
    rbuf = rbuf_drop(rbuf);

    rbuf = rbuf_make(int, 512);
    for (int i = 0; rbuf_push(rbuf, i); i--);
    int j;
    for (int i = 0; rbuf_shift(rbuf, j); i--) {
        assert(i == j);
    }
    rbuf = rbuf_drop(rbuf);

    /* same deal as slice.h unfortunately */
    ringbuf(PTR_OF(int)) *rbufp = rbuf_make(PTR_OF(int), 1);
    assert(!rbuf_push(rbufp, &j));
    int *k;
    assert(rbuf_shift(rbufp, k));
    assert(&j == k);
    rbufp = rbuf_drop(rbufp);

    pthread_t pool[THREADC];
    rbuf = rbuf_make(int, 2);
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, rbuf) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        while (!rbuf_trypush(rbuf, i));
    }
    for (int i = 0; i < THREADC; i++) {
        while (!rbuf_trypush(rbuf, -1));
    }
    unsigned long long sum = 0;
    unsigned long long *r;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ull * 100001ull)/2));

    printf("All tests passed\n");
}
