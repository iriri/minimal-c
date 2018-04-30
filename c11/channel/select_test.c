#include <stdio.h>
#include <pthread.h>
#include "channel.h"

CHANNEL_EXTERN_DECL;

#define THREADC 16

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
            ch_sel_recv(chanpool[0], int, ir);
            assert(ir == 0);
            printf("0, %d\n", ir);
            break;
        case 1:
            ch_sel_recv(chanpool[1], int, ir);
            assert(ir == 1);
            printf("1, %d\n", ir);
            break;
        case 2:
            ch_sel_recv(chanpool[2], int, ir);
            assert(ir == 2);
            printf("2, %d\n", ir);
            break;
        case 3:
            ch_sel_recv(chanpool[3], int, ir);
            assert(ir == 3);
            printf("3, %d\n", ir);
            break;
        case 4:
            ch_sel_recv(chanpool[4], int, ir);
            assert(ir == 4);
            printf("4, %d\n", ir);
            break;
        case 5:
            ch_sel_recv(chanpool[5], int, ir);
            assert(ir == 5);
            printf("5, %d\n", ir);
            break;
        case 6:
            ch_sel_recv(chanpool[6], int, ir);
            assert(ir == 6);
            printf("6, %d\n", ir);
            break;
        case 7:
            ch_sel_recv(chanpool[7], int, ir);
            assert(ir == 7);
            printf("7, %d\n", ir);
            break;
        case 8:
            ch_sel_recv(chanpool[8], int, ir);
            assert(ir == 8);
            printf("8, %d\n", ir);
            break;
        case 9:
            ch_sel_recv(chanpool[9], int, ir);
            assert(ir == 9);
            printf("9, %d\n", ir);
            break;
        case 10:
            ch_sel_recv(chanpool[10], int, ir);
            assert(ir == 10);
            printf("10, %d\n", ir);
            break;
        case 11:
            ch_sel_recv(chanpool[11], int, ir);
            assert(ir == 11);
            printf("11, %d\n", ir);
            break;
        case 12:
            ch_sel_recv(chanpool[12], int, ir);
            assert(ir == 12);
            printf("12, %d\n", ir);
            break;
        case 13:
            ch_sel_recv(chanpool[13], int, ir);
            assert(ir == 13);
            printf("13, %d\n", ir);
            break;
        case 14:
            ch_sel_recv(chanpool[14], int, ir);
            assert(ir == 14);
            printf("14, %d\n", ir);
            break;
        case 15:
            ch_sel_recv(chanpool[15], int, ir);
            assert(ir == 15);
            printf("15, %d\n", ir);
            break;
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
