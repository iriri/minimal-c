#include <stdio.h>
#include <pthread.h>
#include "channel.h"

CHANNEL_EXTERN_DECL;

#define THREADC 16

#define recv_do(id) \
    ch_sel_recv(chanpool[id], int, ir); \
    assert(ir == id); \
    printf("%d, %d\n", id, ir); \
    break

void *
identity(void *arg) {
    channel *chan = (channel *)arg;
    channel *chani;
    assert(ch_recv(chan, channel *, chani) == CH_OK);
    int id;
    assert(ch_recv(chani, int, id) == CH_OK);
    while (ch_send(chani, int, id) != CH_CLOSED) {
        sleep(1);
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
        chanpool[i] = ch_make(int, 4);
        assert(pthread_create(pool + i, NULL, identity, chanp) == 0);
        assert(ch_send(chanp, channel *, chanpool[i]) == CH_OK);
        assert(ch_send(chanpool[i], int, i) == CH_OK);
        ch_sel_reg(sel, chanpool[i], CH_OP_RECV);
    }
    int ir;
    for (int i = 1; i <= 100; i++) {
        switch(ch_select(sel)) {
        case 0:
            recv_do(0);
        case 1:
            recv_do(1);
        case 2:
            recv_do(2);
        case 3:
            recv_do(3);
        case 4:
            recv_do(4);
        case 5:
            recv_do(5);
        case 6:
            recv_do(6);
        case 7:
            recv_do(7);
        case 8:
            recv_do(8);
        case 9:
            recv_do(9);
        case 10:
            recv_do(10);
        case 11:
            recv_do(11);
        case 12:
            recv_do(12);
        case 13:
            recv_do(13);
        case 14:
            recv_do(14);
        case 15:
            recv_do(15);
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
