#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include "../channel/channel.h"

typedef void (*fn)(void);
CHANNEL_DEF(fn);

#define THREADC 128

void *
work(void *arg) {
    channel(fn) *c = (channel(fn) *)arg;
    fn f;

    while (ch_recv(c, f) == CH_OK) {
        f();
    }
    return NULL;
}

void
count(void) {
    static atomic_int i;
    printf("%d\n", atomic_fetch_add_explicit(&i, 1, memory_order_acq_rel));
}

int
main() {
    pthread_t pool[THREADC];
    channel(fn) *work_queue = ch_make(fn, 16);

    for (int i = 0; i < THREADC; i++) {
        pthread_create(pool + i, NULL, work, work_queue);
    }
    for (int i = 0; i < 1024; i++) {
        ch_send(work_queue, count);
    }
    ch_close(work_queue);
    for (int i = 0; i < THREADC; i++) {
        pthread_join(pool[i], NULL);
    }
    ch_drop(work_queue);

    return 0;
}
