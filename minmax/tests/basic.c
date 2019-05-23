#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include "../minmax.h"

MINMAX_EXTERN_DECL;

int
cmp_int(void *restrict i, void *restrict i1) {
    int i_ = *(int *)i, i1_ = *(int *)i1;
    return i_ > i1_ ? 1 : i_ < i1_ ? -1 : 0;
}

void
verify_int_heap(minmax *m) {
    if (mm_len(m) == 0) {
        return;
    }
    int min = INT_MIN, max = INT_MAX;
    mm_peekmin(m, int, &min);
    mm_peekmax(m, int, &max);
    for (size_t i = 0; i < mm_len(m); i++) {
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
        assert(self >= min && self <= max);
    }
}

int
main(void) {
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
    assert(mm_peekmin(m, int, &a));
    assert(a == -999);
    assert(mm_peekmax(m, int, &a));
    assert(a == 999);
    int b = INT_MIN;
    while (mm_pollmin(m, int, &a)) {
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
    while (mm_pollmax(m, int, &a)) {
        assert(a <= b);
        b = a;
    }
    m = mm_drop(m);

    int *ia = malloc(100000 * sizeof(int));
    for (size_t i = 0; i < 100000; i++) {
        ia[i] = rand();
    }
    minmax *m1 = mm_fromarr(int, 100000, 100000, cmp_int, ia);
    verify_int_heap(m1);
    b = INT_MAX;
    for (int i = 0; i < 50000; i++) {
        assert(mm_pollmax(m1, int, &a));
        assert(a <= b);
        b = a;
    }
    verify_int_heap(m1);
    for (int i = 0; i < 100000; i++) {
        mm_insert(m1, int, rand());
    }
    b = INT_MIN;
    while (mm_pollmin(m1, int, &a)) {
        assert(a >= b);
        b = a;
    }
    m1 = mm_drop(m1);

    printf("All tests passed\n");
    return 0;
}
