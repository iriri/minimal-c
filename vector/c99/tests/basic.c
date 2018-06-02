#include <assert.h>
#include <stdio.h>
#include "../vector.h"

VECTOR_EXTERN_DECL;

typedef void (*fn)(void);

void
verify(vector *v, int *ia, size_t len) {
    if (vec_len(v) != len) {
        printf("len mismatch: %lu, %lu\n", vec_len(v), len);
        assert(0);
    }
    for (unsigned i = 0; i < vec_len(v); i++) {
        if (vec_arr(v, int)[i] != ia[i]) {
            printf("%u, %d, %d\n", i, vec_arr(v, int)[i], ia[i]);
            assert(0);
        }
    }
}

void
foo() {
    static int a = 0;
    printf("%d\n", ++a);
}

int
main(void) {
    vector *v = vec_make(int, 0, 2);
    int a = 1;
    vec_push(v, int, a);
    vec_push(v, int, 2);
    assert(vec_pop(v, int, a) && a == 2);
    vec_push(v, int, 2);
    vec_push(v, int, 3);
    assert(v->cap == 4);
    vec_push(v, int, 4);
    vec_push(v, int, 5);
    assert(v->cap == 8);
    int via[] = {1, 2, 3, 4, 5};
    verify(v, via, 5);
    assert(vec_peek(v, int, a));
    assert(a == 5);

    vec_remove(v, 4);
    int via1[] = {1, 2, 3, 4};
    verify(v, via1, 4);
    vec_remove(v, 2);
    int via2[] = {1, 2, 4};
    verify(v, via2, 3);
    a = 1;
    vec_remove(v, vec_find(v, a));
    vec_shrink(v);
    assert(v->cap == 4);
    int via3[] = {2, 4};
    verify(v, via3, 2);
    vec_index(v, int, 0) = 1;
    a = vec_index(v, int, 0);
    assert(a == 1);

    vector *v1 = vec_make(int, 5, 6);
    for (unsigned i = 0; i < vec_len(v1); i++) {
        vec_arr(v1, int)[i] = i + 1;
    }
    verify(v1, via, 5);
    vec_concat(v, v1);
    int via4[] = {1, 4, 1, 2, 3, 4, 5};
    verify(v, via4, 7);

    vec_drop(v);
    vec_drop(v1);

    vector *v2 = vec_make(fn, 0, 2);
    vec_push(v2, fn, &foo);
    vec_push(v2, fn, &foo);
    for (unsigned i = 0; i < vec_len(v2); i++) {
        vec_arr(v2, fn)[i]();
    }
    printf("All tests passed\n");
    return 0;
}
