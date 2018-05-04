#include <stdio.h>
#include <pthread.h>
#include "channel.h"

CHANNEL_EXTERN_DECL;

#define THREADC 16

#define send_do(id) if (1) { \
    int Xi##id = id; \
    ch_sel_send(chanpool[id], int, Xi##id); \
    break; \
}

void *
identity(void *arg) {
    channel *chan = (channel *)arg;
    channel *chani;
    assert(ch_recv(chan, channel *, chani) == CH_OK);
    int id, ir;
    assert(ch_recv(chani, int, id) == CH_OK);
    while (ch_recv(chani, int, ir) != CH_CLOSED) {
        assert(ir == id);
        printf("%d, %d\n", id, ir);
    }
    return NULL;
}

int
main() {
    channel *chanpool[THREADC];
    pthread_t pool[THREADC];
    channel *chanp = ch_make(channel *, 0);
    channel_select *sel = ch_sel_init(THREADC);
    for (int i = 0; i < THREADC; i++) {
        chanpool[i] = ch_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, chanp) == 0);
        assert(ch_send(chanp, channel *, chanpool[i]) == CH_OK);
        assert(ch_send(chanpool[i], int, i) == CH_OK);
        ch_sel_reg(sel, chanpool[i], CH_OP_SEND);
    }
    for (int i = 1; i <= 100; i++) {
        switch(ch_select(sel)) {
        case 0:
            send_do(0);
        case 1:
            send_do(1);
        case 2:
            send_do(2);
        case 3:
            send_do(3);
        case 4:
            send_do(4);
        case 5:
            send_do(5);
        case 6:
            send_do(6);
        case 7:
            send_do(7);
        case 8:
            send_do(8);
        case 9:
            send_do(9);
        case 10:
            send_do(10);
        case 11:
            send_do(11);
        case 12:
            send_do(12);
        case 13:
            send_do(13);
        case 14:
            send_do(14);
        case 15:
            send_do(15);
        default:
            assert(false);
        }
    }
    sel = ch_sel_finish(sel);
    for (int i = 0; i < THREADC; i++) {
        ch_close(chanpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        chanpool[i] = ch_drop(chanpool[i]);
    }
    chanp = ch_drop(chanp);

    printf("All tests passed\n");
}
