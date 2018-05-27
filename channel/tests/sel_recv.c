#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include "../channel.h"

CHANNEL_EXTERN_DECL;

#define THREADC 16

void *
identity(void *arg) {
    channel *chan = (channel *)arg;
    channel *chani;
    assert(ch_recv(chan, chani) == CH_OK);
    int id;
    assert(ch_recv(chani, id) == CH_OK);
    while (ch_send(chani, id) != CH_CLOSED) {
        sleep(1);
    }
    return NULL;
}

int
main() {
    srand(time(NULL));

    channel *chanpool[THREADC];
    pthread_t pool[THREADC];
    channel *chanp = ch_make(channel *, 0);
    int ir;
    channel_set *set = ch_set_make(THREADC);
    for (int i = 0; i < THREADC; i++) {
        chanpool[i] = ch_make(int, 1);
        assert(pthread_create(pool + i, NULL, identity, chanp) == 0);
        assert(ch_send(chanp, chanpool[i]) == CH_OK);
        assert(ch_send(chanpool[i], i) == CH_OK);
        ch_set_add(set, chanpool[i], CH_RECV, ir);
    }
    for (int i = 1; i <= 100; i++) {
        int id = ch_select(set);
        printf("%d, %d\n", id, ir);
        assert(ir == id);
    }
    set = ch_set_drop(set);
    for (int i = 0; i < THREADC; i++) {
        ch_close(chanpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        chanpool[i] = ch_drop(chanpool[i]);
    }
    chanp = ch_drop(chanp);

    printf("All tests passed\n");
}
