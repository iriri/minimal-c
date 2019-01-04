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
identity(void *arg) {
    channel(ptr(channel(int))) *chan = (channel(ptr(channel(int))) *)arg;
    channel(int) *chani;
    assert(ch_recv(chan, chani) == CH_OK);
    int id, ir;
    assert(ch_recv(chani, id) == CH_OK);
    while (ch_recv(chani, ir) != CH_CLOSED) {
        printf("%d, %d\n", id, ir);
        assert(ir == id);
        usleep(10000);
    }
    return NULL;
}

int
main(void) {
    srand(time(NULL));

    channel(int) *chanpool[THREADC];
    pthread_t pool[THREADC];
    channel(ptr(channel(int))) *chanp = ch_make(ptr(channel(int)), 0);
    int ia[THREADC];
    channel_set *set = ch_set_make(1);
    for (int i = 0; i < THREADC; i++) {
        chanpool[i] = ch_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, chanp) == 0);
        assert(ch_send(chanp, chanpool[i]) == CH_OK);
        assert(ch_send(chanpool[i], i) == CH_OK);
        ia[i] = i;
        ch_set_add(set, chanpool[i], CH_SEND, ia[i]);
    }
    for (int i = 1; i <= 100; i++) {
        ch_select(set);
    }
    set = ch_set_drop(set);
    for (int i = 0; i < THREADC; i++) {
        ch_close(chanpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        chanpool[i] = ch_drop(chanpool[i]);
    }
    chanp = ch_drop(chanp);

    printf("All tests passed\n");
    return 0;
}
