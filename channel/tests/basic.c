#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include "../channel.h"

CHANNEL_EXTERN_DECL;

#define THREADC 16

void *
adder(void *arg) {
    channel *chan = (channel *)arg;
    int i;
    long long sum = 0;
    while (ch_recv(chan, &i) != CH_CLOSED) {
        sum += i;
    }
    long long *ret = malloc(sizeof(*ret));
    *ret = sum;
    printf("%llu\n", sum);
    return ret;
}

void *
identity(void *arg) {
    channel *chan = (channel *)arg;
    channel *chani;
    assert(ch_recv(chan, &chani) == CH_OK);
    int id;
    assert(ch_recv(chani, &id) == CH_OK);
    while (ch_send(chani, &id) != CH_CLOSED);
    return NULL;
}

channel_rc
forcesend(channel *c, int i) {
    channel_rc rc;
    while ((rc = ch_trysend(c, &i)) == CH_WBLOCK) {
        int j;
        ch_tryrecv(c, &j);
    }
    return rc;
}

int
main(void) {
    srand(time(NULL));

    int i;
    channel *chan = ch_make(int, 2);
    i = 1;
    assert(ch_send(chan, &i) == CH_OK);
    i = 2;
    assert(ch_send(chan, &i) == CH_OK);
    /* ch_trysend does not block but immediately returns with CH_WBLOCK */
    i = 3;
    assert(ch_trysend(chan, &i) == CH_WBLOCK);
    /* ch_forcesend has been removed but the semantics are easy to replicate */
    assert(forcesend(chan, i) == CH_OK);
    assert(ch_tryrecv(chan, &i) == CH_OK);
    assert(i == 2);
    assert(ch_recv(chan, &i) == CH_OK);
    assert(i == 3);
    assert(ch_tryrecv(chan, &i) == CH_WBLOCK); /* Same deal as ch_trysend */
    assert(i == 3);
    /* ch_open increments the open count and returns the same channel. ch_close
     * decrements the count and only actually closes the channel if the count
     * is now 0. */
    channel *chan1 = ch_open(chan);
    ch_close(chan1);
    channel *chan2 = ch_open(ch_dup(chan1));
    ch_close(chan);
    ch_close(chan2);
    /* This library aims to better support closing channels from the receiver
     * side so sends can fail gracefully with CH_CLOSED */
    i = 1;
    assert(ch_send(chan, &i) == CH_CLOSED);
    ch_drop(chan);
    assert(ch_send(chan2, &i) == CH_CLOSED);
    ch_drop(chan2);

    chan = ch_make(int, 1);
    pthread_t pool[THREADC];
    for (size_t i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, chan) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        ch_send(chan, &i);
    }
    ch_close(chan);
    long long sum = 0;
    long long *r;
    for (size_t i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ll * 100001ll)/2));
    ch_drop(chan);

    chan = ch_make(int, 0);
    for (size_t i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, chan) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        assert(ch_send(chan, &i) == CH_OK);
    }
    ch_close(chan);
    sum = 0;
    for (size_t i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ll * 100001ll)/2));
    ch_drop(chan);

    channel *chanpool[THREADC];
    channel *chanp = ch_make(channel *, 0);
    int ir;
    channel_case cases[THREADC];
    for (int i = 0; i < THREADC; i++) {
        chanpool[i] = ch_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, chanp) == 0);
        assert(ch_send(chanp, &chanpool[i]) == CH_OK);
        assert(ch_send(chanpool[i], &i) == CH_OK);
        cases[i] = (const channel_case){
            .c = chanpool[i], .msg = &ir, .op = CH_RECV
        };
    }

    for (int i = 1; i <= 100; i++) {
        /* kill me now */
        switch (ch_alt(cases, THREADC)) {
        case 0:
            assert(ir == 0);
            break;
        case 1:
            assert(ir == 1);
            break;
        case 2:
            assert(ir == 2);
            break;
        case 3:
            assert(ir == 3);
            break;
        case 4:
            assert(ir == 4);
            break;
        case 5:
            assert(ir == 5);
            break;
        case 6:
            assert(ir == 6);
            break;
        case 7:
            assert(ir == 7);
            break;
        case 8:
            assert(ir == 8);
            break;
        case 9:
            assert(ir == 9);
            break;
        case 10:
            assert(ir == 10);
            break;
        case 11:
            assert(ir == 11);
            break;
        case 12:
            assert(ir == 12);
            break;
        case 13:
            assert(ir == 13);
            break;
        case 14:
            assert(ir == 14);
            break;
        case 15:
            assert(ir == 15);
            break;
        default: assert(false);
        }
    }
    for (size_t i = 0; i < THREADC; i++) {
        ch_close(chanpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        chanpool[i] = ch_drop(chanpool[i]);
    }
    chanp = ch_drop(chanp);

    printf("All tests passed\n");
    return 0;
}
