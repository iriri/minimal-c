#include <stdio.h>
#include "slice.h"

extern inline slice slice_make(size_t, size_t, size_t);
extern inline slice slice_slice(slice *, size_t, size_t, size_t, bool);
extern inline void slice_append(slice *, size_t, void *);
extern inline void slice_remove(slice *, size_t, size_t);

void
verify_slice(slice s, int *ia) {
    for (unsigned i = 0; i < s.len; i++) {
        if (INDEX(s, int, i) != ia[i]) {
            printf("%u, %d, %d\n", i, INDEX(s, int, i), ia[i]);
            assert(false);
        }
    }
}

int
main() {
    slice s = MAKE(int, 0, 2);
    int a = 1;
    APPEND(s, int, a);
    APPEND(s, int, 2);
    APPEND(s, int, 3);
    APPEND(s, int, 4);
    APPEND(s, int, 5);
    int sia[] = {1, 2, 3, 4, 5};
    verify_slice(s, sia);

    slice s1 = SLICE(s, int, 0, 5);
    assert(s1.len == 5);
    slice s2 = SLICE(s, int, 3, 4);
    assert(s2.len == 1);
    assert(s2.base->cap == 8);
    int s1ia[] = {1, 2, 3, 4, 5};
    verify_slice(s1, s1ia);
    int s2ia[] = {4};
    verify_slice(s2, s2ia);
    APPEND(s2, int, 5);
    APPEND(s2, int, 6);
    APPEND(s2, int, 7);
    APPEND(s2, int, 8);
    assert(s2.base->cap == 8);
    APPEND(s2, int, 9);
    assert(s2.base->cap == 16);
    int s2ia1[] = {4, 5, 6, 7, 8, 9};
    verify_slice(s2, s2ia1);

    slice s3 = SLICE_NEW(s2, int, 1, 4);
    assert(s3.len == 3);
    assert(s3.base->cap == 6);
    int s3ia[] = {5, 6, 7};
    verify_slice(s3, s3ia);
    assert(GET(s3, int, 1) == INDEX(s3, int, 1));
    INDEX(s3, int, 1) = 1234;
    assert(GET(s3, int, 1) == 1234);
    PUT(s3, int, 2, 5678);
    assert(GET(s3, int, 2) == 5678);
    APPEND(s3, int, 9);
    int s3ia1[] = {5, 1234, 5678, 9};
    verify_slice(s3, s3ia1);

    REMOVE(s3, int, 1);
    int s3ia2[] = {5, 5678, 9};
    verify_slice(s3, s3ia2);
    REMOVE(s3, int, 0);
    int s3ia3[] = {5678, 9};
    verify_slice(s3, s3ia3);
    REMOVE(s3, int, 1);
    int s3ia4[] = {5678};
    verify_slice(s3, s3ia4);

    slice s4 = SLICE(s2, int, 3, 5);
    int s4ia[] = {7, 8};
    verify_slice(s4, s4ia);
    memset(ARR(s4), 0, s4.len * sizeof(int));
    int s4ia1[] = {0, 0};
    verify_slice(s4, s4ia1);
    int s2ia2[] = {4, 5, 6, 0, 0, 9};
    verify_slice(s2, s2ia2);

    DROP(s);
    DROP(s1);
    DROP(s2);
    DROP(s3);
    DROP(s4);
    printf("All tests passed\n");
}
