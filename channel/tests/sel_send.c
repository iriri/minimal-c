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
    assert(ch_recv(chan, &chani) == CH_OK);
    int id, ir;
    assert(ch_recv(chani, &id) == CH_OK);
    while (ch_recv(chani, &ir) != CH_CLOSED) {
        printf("%d, %d\n", id, ir);
        assert(ir == id);
        usleep(10000);
    }
    return NULL;
}

int
main(void) {
    srand(time(NULL));

    channel *chanpool[THREADC];
    pthread_t pool[THREADC];
    channel *chanp = ch_make(channel *, 0);
    int ia[THREADC];
    channel_case cases[THREADC];
    for (int i = 0; i < THREADC; i++) {
        chanpool[i] = ch_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, chanp) == 0);
        assert(ch_send(chanp, &chanpool[i]) == CH_OK);
        assert(ch_send(chanpool[i], &i) == CH_OK);
        ia[i] = i;
        cases[i] = (const channel_case){
            .c = chanpool[i], .msg = &ia[i], .op = CH_SEND
        };
    }
    for (int i = 1; i <= 100; i++) {
        ch_alt(cases, THREADC);
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
