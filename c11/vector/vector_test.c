#include "vector.h"
#include <stdio.h>

VECTOR_EXTERN_DECL;

void
verify(vector v, int *ia, size_t len) {
    if (v.len != len) {
        printf("len mismatch: %lu, %lu\n", v.len, len);
        assert(0);
    }
    for (unsigned i = 0; i < v.len; i++) {
        if (vec_arr(v, int)[i] != ia[i]) {
            printf("%u, %d, %d\n", i, vec_arr(v, int)[i], ia[i]);
            assert(0);
        }
    }
}

int
main() {
    vector v = vec_alloc(int, 0, 2);
    int a = 1;
    vec_append(v, int, a);
    vec_append(v, int, 2);
    vec_append(v, int, 3);
    assert(v.cap == 4);
    vec_append(v, int, 4);
    vec_append(v, int, 5);
    assert(v.cap == 8);
    int via[] = {1, 2, 3, 4, 5};
    verify(v, via, 5);

    vec_remove(v, int, 4);
    int via1[] = {1, 2, 3, 4};
    verify(v, via1, 4);
    vec_remove(v, int, 2);
    int via2[] = {1, 2, 4};
    verify(v, via2, 3);
    vec_remove(v, int, vec_find(v, int, a));
    int via3[] = {2, 4};
    verify(v, via3, 2);
    vec_put(v, int, 0, 1);
    assert(vec_get(v, int, 0) == 1);

    vector v1 = vec_alloc(int, 5, 6);
    for (unsigned i = 0; i < v1.len; i++) {
        vec_arr(v1, int)[i] = i + 1;
    }
    verify(v1, via, 5);
    vec_concat(v, int, v1);
    int via4[] = {1, 4, 1, 2, 3, 4, 5};
    verify(v, via4, 7);

    vec_free(v);
    vec_free(v1);

    /*
    vector v2 = vec_alloc(int, 0, 512);
    for (int i = 0; i < 1000000000; i++) {
        vec_append(v2, int, i);
    }
    printf("%d\n", vec_get(v2, int, 256));
    printf("%d\n", vec_get(v2, int, 555555));
    printf("%d\n", vec_get(v2, int, 999999999));
    vec_free(v2);
    */

    printf("All tests passed\n");
}
