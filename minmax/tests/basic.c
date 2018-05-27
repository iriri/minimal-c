#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include "../minmax.h"

MINMAX_EXTERN_DECL;

int
cmp_int(void *restrict i, void *restrict i1) {
    // printf("comparing %d and %d\n", *(int *)i, *(int *)i1);
    return *(int *)i - *(int *)i1;
}

void
verify_int_heap(minmax *m) {
    if (m->len == 0) {
        return;
    }
    for (unsigned i = 0; i < m->len; i++) {
        if (minmax_parent_(i) == SIZE_MAX) {
            continue;
        }
        int parent = *(int *)(m->heap + (minmax_parent_(i) * sizeof(int)));
        int self = *(int *)(m->heap + (i * sizeof(int)));
        if (minmax_level_type_max_(i)) {
            assert(self >= parent);
        } else {
            assert(self <= parent);
        }
    }
}

int
main() {
    srand(time(NULL));

    minmax *m = mm_make(int, 1, cmp_int);
    for (int i = -999; i < 1000; i++) {
        mm_insert(m, int, i);
    }
    for (int i = -999; i < 1000; i++) {
        mm_insert(m, int, i);
    }
    verify_int_heap(m);
    int a;
    assert(mm_peekmin(m, int, a));
    assert(a == -999);
    assert(mm_peekmax(m, int, a));
    assert(a == 999);
    int b = INT_MIN;
    while (mm_pollmin(m, int, a)) {
        verify_int_heap(m);
        assert(a >= b);
        b = a;
    }
    m = mm_drop(m);

    m = mm_make(int, 512, cmp_int);
    for (int i = 0; i < 1000000; i++) {
        mm_insert(m, int, rand());
    }
    verify_int_heap(m);
    b = INT_MAX;
    while (mm_pollmax(m, int, a)) {
        // verify_int_heap(m);
        assert(a <= b);
        b = a;
    }
    m = mm_drop(m);

    printf("All tests passed\n");
}
