#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "../minmax.h"

MINMAX_EXTERN_DECL;

int
cmp_long(void *restrict l, void *restrict l1) {
    long l_ = *(long *)l, l1_ = *(long *)l1;
    return l_ > l1_ ? 1 : l_ < l1_ ? -1 : 0;
}

void
verify_long_heap(minmax *m) {
    if (mm_len(m) == 0) {
        return;
    }
    long min = LONG_MIN, max = LONG_MAX;
    mm_peekmin(m, long, &min);
    mm_peekmax(m, long, &max);
    for (size_t i = 0; i < mm_len(m); i++) {
        if (minmax_parent_(i) == SIZE_MAX) {
            continue;
        }
        long parent = *(long *)(m->heap + (minmax_parent_(i) * sizeof(long)));
        long self = *(long *)(m->heap + (i * sizeof(long)));
        if (minmax_level_type_max_(i)) {
            assert(self >= parent);
        } else {
            assert(self <= parent);
        }
        assert(self >= min && self <= max);
    }
}

int
main(void) {
    minmax *m = mm_make(long, 32, cmp_long);
    char *buf = NULL;
    size_t bufcap = 0;
    for ( ; ; ) {
        ssize_t readlen = getline(&buf, &bufcap, stdin);
        if (readlen < 0) {
            break;
        }
        long l = strtol(buf, NULL, 0);
        if (l == -1) {
            mm_pollmin(m, long, &l);
        } else if (l == 1) {
            mm_pollmax(m, long, &l);
        } else {
            mm_insert(m, long, l);
        }
        verify_long_heap(m);
    }
    free(buf);
    buf = NULL;
    bufcap = 0;
    mm_drop(m);
    return 0;
}
