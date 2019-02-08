#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include "../channel.h"

CHANNEL_EXTERN_DECL;

CHANNEL_DEF(int);
CHANNEL_DEF_PTR(channel(int));

#define THREADC 16

void *
adder(void *arg) {
    channel(int) *chan = (channel(int) *)arg;
    int i;
    long long sum = 0;
    while (ch_recv(chan, i) != CH_CLOSED) {
        sum += i;
    }
    long long *ret = malloc(sizeof(*ret));
    *ret = sum;
    printf("%llu\n", sum);
    return ret;
}

void *
identity(void *arg) {
    channel(ptr(channel(int))) *chan = (channel(ptr(channel(int))) *)arg;
    channel(int) *chani;
    assert(ch_recv(chan, chani) == CH_OK);
    int id;
    assert(ch_recv(chani, id) == CH_OK);
    while (ch_send(chani, id) != CH_CLOSED);
    return NULL;
}

channel_rc
forcesend(channel(int) *c, int i) {
    channel_rc rc;
    while ((rc = ch_trysend(c, i)) == CH_WBLOCK) {
        int j;
        ch_tryrecv(c, j);
    }
    return rc;
}

int
main(void) {
    srand(time(NULL));

    int i;
    channel(int) *chan = ch_make(int, 2);
    i = 1;
    assert(ch_send(chan, i) == CH_OK);
    i = 2;
    assert(ch_send(chan, i) == CH_OK);
    /* ch_trysend does not block but immediately returns with CH_WBLOCK */
    i = 3;
    assert(ch_trysend(chan, i) == CH_WBLOCK);
    /* ch_forcesend has been removed but the semantics are easy to replicate */
    assert(forcesend(chan, i) == CH_OK);
    assert(ch_tryrecv(chan, i) == CH_OK);
    assert(i == 2);
    assert(ch_recv(chan, i) == CH_OK);
    assert(i == 3);
    assert(ch_tryrecv(chan, i) == CH_WBLOCK); /* Same deal as ch_trysend */
    assert(i == 3);
    /* ch_open increments the open count and returns the same channel. ch_close
     * decrements the count and only actually closes the channel if the count
     * is now 0. */
    channel(int) *chan1 = ch_open(chan);
    ch_close(chan1);
    channel(int) *chan2 = ch_open(ch_dup(chan1));
    ch_close(chan);
    ch_close(chan2);
    /* This library aims to better support closing channels from the receiver
     * side so sends can fail gracefully with CH_CLOSED */
    i = 1;
    assert(ch_send(chan, i) == CH_CLOSED);
    ch_drop(chan);
    assert(ch_send(chan2, i) == CH_CLOSED);
    ch_drop(chan2);

    chan = ch_make(int, 1);
    pthread_t pool[THREADC];
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, chan) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        ch_send(chan, i);
    }
    ch_close(chan);
    long long sum = 0;
    long long *r;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ll * 100001ll)/2));
    ch_drop(chan);

    chan = ch_make(int, 0);
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, chan) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        assert(ch_send(chan, i) == CH_OK);
    }
    ch_close(chan);
    sum = 0;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ll * 100001ll)/2));
    ch_drop(chan);

    channel(int) *chanpool[THREADC];
    channel(ptr(channel(int))) *chanp = ch_make(ptr(channel(int)), 0);
    for (int i = 0; i < THREADC; i++) {
        chanpool[i] = ch_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, chanp) == 0);
        assert(ch_send(chanp, chanpool[i]) == CH_OK);
        assert(ch_send(chanpool[i], i) == CH_OK);
    }
    int ir;
    for (int i = 1; i <= 100; i++) {
        /* kill me now */
        ch_poll(THREADC) {
            ch_case(0, ch_tryrecv(chanpool[0], ir), assert(ir == 0));
            ch_case(1, ch_tryrecv(chanpool[1], ir), { /* also works */
                    printf("1, %d\n", ir);
                    assert(ir == 1);
            });
            ch_case(2, ch_tryrecv(chanpool[2], ir), assert(ir == 2));
            ch_case(3, ch_tryrecv(chanpool[3], ir), assert(ir == 3));
            ch_case(4, ch_tryrecv(chanpool[4], ir), assert(ir == 4));
            ch_case(5, ch_tryrecv(chanpool[5], ir), assert(ir == 5));
            ch_case(6, ch_tryrecv(chanpool[6], ir), assert(ir == 6));
            ch_case(7, ch_tryrecv(chanpool[7], ir), assert(ir == 7));
            ch_case(8, ch_tryrecv(chanpool[8], ir), assert(ir == 8));
            ch_case(9, ch_tryrecv(chanpool[9], ir), assert(ir == 9));
            ch_case(10, ch_tryrecv(chanpool[10], ir), assert(ir == 10));
            ch_case(11, ch_tryrecv(chanpool[11], ir), assert(ir == 11));
            ch_case(12, ch_tryrecv(chanpool[12], ir), assert(ir == 12));
            ch_case(13, ch_tryrecv(chanpool[13], ir), assert(ir == 13));
            ch_case(14, ch_tryrecv(chanpool[14], ir), assert(ir == 14));
            ch_case(15, ch_tryrecv(chanpool[15], ir), assert(ir == 15));
            ch_default({
                printf("default\n");
            });
        } ch_poll_end;
    }
    for (int i = 0; i < THREADC; i++) {
        ch_close(chanpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        chanpool[i] = ch_drop(chanpool[i]);
    }
    chanp = ch_drop(chanp);

    printf("All tests passed\n");
    return 0;
}