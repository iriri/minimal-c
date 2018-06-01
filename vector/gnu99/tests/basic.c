#include <assert.h>
#include <stdio.h>
#include "../vector.h"

VECTOR_EXTERN_DECL;

typedef void (*fn)(void);
VECTOR_DEF(int);
VECTOR_DEF(fn);

void
verify(vector(int) *v, int *ia, size_t len) {
    if (vec_len(v) != len) {
        printf("len mismatch: %lu, %lu\n", vec_len(v), len);
        assert(0);
    }
    for (unsigned i = 0; i < vec_len(v); i++) {
        if (vec_arr(v)[i] != ia[i]) {
            printf("%u, %d, %d\n", i, vec_arr(v)[i], ia[i]);
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
main() {
    vector(int) *v = vec_make(int, 0, 2);
    int a = 1;
    vec_push(v, a);
    vec_push(v, 2);
    assert(vec_pop(v, a) && a == 2);
    vec_push(v, 2);
    vec_push(v, 3);
    assert(v->vec.cap == 4);
    vec_push(v, 4);
    vec_push(v, 5);
    assert(v->vec.cap == 8);
    int via[] = {1, 2, 3, 4, 5};
    verify(v, via, 5);
    assert(vec_peek(v, a));
    assert(a == 5);

    vec_remove(v, 4);
    int via1[] = {1, 2, 3, 4};
    verify(v, via1, 4);
    vec_remove(v, 2);
    int via2[] = {1, 2, 4};
    verify(v, via2, 3);
    vec_remove(v, vec_find(v, 1));
    vec_shrink(v);
    assert(v->vec.cap == 4);
    int via3[] = {2, 4};
    verify(v, via3, 2);
    vec_index(v, 0) = 1;
    a = vec_index(v, 0);
    assert(a == 1);

    vector(int) *v1 = vec_make(int, 5, 6);
    for (unsigned i = 0; i < vec_len(v1); i++) {
        vec_arr(v1)[i] = i + 1;
    }
    verify(v1, via, 5);
    vec_concat(v, v1);
    int via4[] = {1, 4, 1, 2, 3, 4, 5};
    verify(v, via4, 7);

    vec_drop(v);
    vec_drop(v1);

    vector(fn) *v2 = vec_make(fn, 0, 2);
    vec_push(v2, &foo);
    vec_push(v2, &foo);
    for (unsigned i = 0; i < vec_len(v2); i++) {
        vec_arr(v2)[i]();
    }
    printf("All tests passed\n");
}
