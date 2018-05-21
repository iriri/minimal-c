#include <assert.h>
#include <stdio.h>
#include "../vector.h"

VECTOR_EXTERN_DECL;

void
verify(vector *v, int *ia, size_t len) {
    if (v->len != len) {
        printf("len mismatch: %lu, %lu\n", v->len, len);
        assert(0);
    }
    for (unsigned i = 0; i < v->len; i++) {
        if (vec_arr(v, int)[i] != ia[i]) {
            printf("%u, %d, %d\n", i, vec_arr(v, int)[i], ia[i]);
            assert(0);
        }
    }
}

int
main() {
    vector *v = vec_make(int, 0, 2);
    int a = 1;
    vec_append(v, a);
    vec_append(v, 2);
    vec_append(v, 3);
    assert(v->cap == 4);
    vec_append(v, 4);
    vec_append(v, 5);
    assert(v->cap == 8);
    int via[] = {1, 2, 3, 4, 5};
    verify(v, via, 5);

    vec_remove(v, 4);
    int via1[] = {1, 2, 3, 4};
    verify(v, via1, 4);
    vec_remove(v, 2);
    int via2[] = {1, 2, 4};
    verify(v, via2, 3);
    vec_remove(v, vec_find(v, a));
    int via3[] = {2, 4};
    verify(v, via3, 2);
    vec_put(v, 0, 1);
    vec_get(v, 0, a);
    assert(a == 1);

    vector *v1 = vec_make(int, 5, 6);
    for (unsigned i = 0; i < v1->len; i++) {
        vec_arr(v1, int)[i] = i + 1;
    }
    verify(v1, via, 5);
    vec_concat(v, v1);
    int via4[] = {1, 4, 1, 2, 3, 4, 5};
    verify(v, via4, 7);

    vec_drop(v);
    vec_drop(v1);
    printf("All tests passed\n");
}
