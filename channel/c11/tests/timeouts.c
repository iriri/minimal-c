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
    while (ch_recv(chan, i) != CH_CLOSED) {
        sum += i;
        usleep(1000);
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
    assert(ch_recv(chan, chani) == CH_OK);
    int id;
    assert(ch_recv(chani, id) == CH_OK);
    while (ch_send(chani, id) != CH_CLOSED) {
        usleep(1000);
    }
    return NULL;
}

int
main(void) {
    srand(time(NULL));

    channel *chan = ch_make(int, 1);
    pthread_t pool[THREADC];
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, chan) == 0);
    }
    int ok = 0, timedout = 0;
    for (int i = 1; i <= 10000; i++) {
        if (ch_timedsend(chan, i, 100) == CH_OK) {
            ok++;
        } else {
            timedout++;
        }
    }
    ch_close(chan);
    long long sum = 0;
    long long *r;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("ok: %d timedout: %d\n", ok, timedout);
    printf("%llu\n", sum);
    ch_drop(chan);

    chan = ch_make(int, 0);
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, chan) == 0);
    }
    ok = 0, timedout = 0;
    for (int i = 1; i <= 10000; i++) {
        if (ch_timedsend(chan, i, 100) == CH_OK) {
            ok++;
        } else {
            timedout++;
        }
    }
    ch_close(chan);
    sum = 0;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("ok: %d timedout: %d\n", ok, timedout);
    printf("%llu\n", sum);
    ch_drop(chan);

    channel *chanpool[THREADC];
    channel *chanp = ch_make(channel *, 0);
    int ir;
    int stats[THREADC] = {0};
    channel_set *set = ch_set_make(THREADC);
    for (int i = 0; i < THREADC; i++) {
        chanpool[i] = ch_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, chanp) == 0);
        assert(ch_send(chanp, chanpool[i]) == CH_OK);
        assert(ch_send(chanpool[i], i) == CH_OK);
        ch_set_add(set, chanpool[i], CH_RECV, ir);
    }
    ok = timedout = 0;
    for (int i = 1; i <= 10000; i++) {
        uint32_t id = ch_timedselect(set, 100);
        if (id != CH_WBLOCK) {
            ok++;
            assert((unsigned)ir == id);
            stats[ir]++;
        } else {
            timedout++;
        }
    }
    set = ch_set_drop(set);
    for (int i = 0; i < THREADC; i++) {
        ch_close(chanpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        chanpool[i] = ch_drop(chanpool[i]);
        printf("%d: %d ", i, stats[i]);
    }
    chanp = ch_drop(chanp);
    printf("\b\nok: %d timedout: %d\n", ok, timedout);

    printf("All tests passed\n");
    return 0;
}
