#include <stdio.h>
#include <pthread.h>
#include "channel.h"

CHANNEL_EXTERN_DECL;

#define THREADC 16

void *
adder(void *arg) {
    channel *chan = (channel *)arg;
    int i;
    unsigned long long sum = 0;
    while (ch_recv(chan, int, i) != CH_CLOSED) {
        sum += i;
    }
    unsigned long long *ret = malloc(sizeof(*ret));
    *ret = sum;
    printf("%llu\n", sum);
    return ret;
}

void *
identity(void *arg) {
    channel *chan = (channel *)arg;
    channel *chani;
    assert(ch_recv(chan, channel *, chani) == CH_OK);
    int id;
    assert(ch_recv(chani, int, id) == CH_OK);
    while (ch_send(chani, int, id) != CH_CLOSED);
    return NULL;
}

int
main() {
    int i;
    channel *chan= ch_make(int, 2);
    i = 1;
    assert(ch_send(chan, int, i) == CH_OK);
    i = 2;
    assert(ch_send(chan, int, i) == CH_OK);
    /* ch_trysend does not block but immediately returns with CH_FULL */
    i = 3;
    assert(ch_trysend(chan, int, i) == CH_FULL);
    /* ch_forcesend is also non-blocking but just overwrites the oldest value
     * instead. Does not work with unbuffered channels. */
    assert(ch_forcesend(chan, int, i) == CH_FULL);
    assert(ch_tryrecv(chan, int, i) == CH_OK);
    assert(i == 2);
    assert(ch_recv(chan, int, i) == CH_OK);
    assert(i == 3);
    assert(ch_tryrecv(chan, int, i) == CH_EMPTY); /* Same deal as ch_trysend */
    assert(i == 3);
    /* Added to make the multiple producer case less painful. Dup increments a
     * reference count and returns the same channel. ch_close decrements the
     * count and only actually closes the channel if the count is now 0. */
    channel *dup = ch_dup(chan);
    ch_close(dup);
    channel *dup1 = ch_dup(dup);
    ch_close(chan);
    assert(!chan->closed);
    ch_close(dup1);
    assert(chan->closed);
    /* This library aims to better support closing channels from the receiver
     * side so sends can fail gracefully with CH_CLOSED */
    i = 1;
    assert(ch_send(chan, int, i) == CH_CLOSED);
    ch_drop(chan);

    chan = ch_make(int, 0);
    pthread_t pool[THREADC];
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, chan) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        ch_send(chan, int, i);
    }
    ch_close(chan);
    unsigned long long sum = 0;
    unsigned long long *r;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ull * 100001ull)/2));
    ch_drop(chan);

    chan = ch_make(int, 0);
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, chan) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        assert(ch_send(chan, int, i) == CH_OK);
    }
    ch_close(chan);
    sum = 0;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ull * 100001ull)/2));
    ch_drop(chan);

    channel *chanpool[THREADC];
    channel *chanp = ch_make(channel *, 0);
    for (int i = 0; i < THREADC; i++) {
        chanpool[i] = ch_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, chanp) == 0);
        assert(ch_send(chanp, channel *, chanpool[i]) == CH_OK);
        assert(ch_send(chanpool[i], int, i) == CH_OK);
    }
    int ir;
    for (int i = 1; i <= 100; i++) {
        /* kill me now */
        ch_select(THREADC) {
            ch_case(0, ch_tryrecv(chanpool[0], int, ir), assert(ir == 0));
            ch_case(1, ch_tryrecv(chanpool[1], int, ir), { /* also works */
                    printf("1, %d\n", ir);
                    assert(ir == 1);
            });
            ch_case(2, ch_tryrecv(chanpool[2], int, ir), assert(ir == 2));
            ch_case(3, ch_tryrecv(chanpool[3], int, ir), assert(ir == 3));
            ch_case(4, ch_tryrecv(chanpool[4], int, ir), assert(ir == 4));
            ch_case(5, ch_tryrecv(chanpool[5], int, ir), assert(ir == 5));
            ch_case(6, ch_tryrecv(chanpool[6], int, ir), assert(ir == 6));
            ch_case(7, ch_tryrecv(chanpool[7], int, ir), assert(ir == 7));
            ch_case(8, ch_tryrecv(chanpool[8], int, ir), assert(ir == 8));
            ch_case(9, ch_tryrecv(chanpool[9], int, ir), assert(ir == 9));
            ch_case(10, ch_tryrecv(chanpool[10], int, ir), assert(ir == 10));
            ch_case(11, ch_tryrecv(chanpool[11], int, ir), assert(ir == 11));
            ch_case(12, ch_tryrecv(chanpool[12], int, ir), assert(ir == 12));
            ch_case(13, ch_tryrecv(chanpool[13], int, ir), assert(ir == 13));
            ch_case(14, ch_tryrecv(chanpool[14], int, ir), assert(ir == 14));
            ch_case(15, ch_tryrecv(chanpool[15], int, ir), assert(ir == 15));
            ch_default({
                printf("default\n");
            });
        } ch_select_end
    }
    for (int i = 0; i < THREADC; i++) {
        ch_close(chanpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        chanpool[i] = ch_drop(chanpool[i]);
    }
    printf("dear POSIX committee please add pthread_mutex_setclock\n");
    ch_timedrecv(chanp, channel *, 123, chan);
    printf("so timed sends and receives can use CLOCK_MONOTIME\n");
    ch_timedrecv(chanp, channel *, 2468, chan);
    printf("and don't make it optional so Apple actually implements it\n");

    chanp = ch_drop(chanp);

    printf("All tests passed\n");
}
