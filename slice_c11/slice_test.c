/* test should be compiled with -fsanitize=address */
#include <stdio.h>
#include "slice.h"

/* C99 inline rules are a little weird. When a function is defined as "inline"
 * in a header file, one "extern" declaration in one (and only one) translation
 * unit is needed to actually generate the non-inline version of that function.
 * Defining functions this way in a single header library, as opposed to with
 * "static inline", requires this small bit of extra work on the part of the
 * user but avoids code duplication when the header is included in multiple
 * translation units. This macro also includes a definition for a struct used
 * by by SLICE_OF when it is called on an array */
SLICE_EXTERN_DECL;

void
verify_slice(slice *s, int *ia, size_t len) {
    if (s->len != len) {
        printf("len mismatch: %lu, %lu\n", s->len, len);
        assert(0);
    }
    for (unsigned i = 0; i < s->len; i++) {
        if (INDEX(s, int, i) != ia[i]) {
            printf("%u, %d, %d\n", i, INDEX(s, int, i), ia[i]);
            assert(0);
        }
    }
}

int
main() {
    slice *s = MAKE(int, 0, 2); /* empty slice with initial capacity 2 */
    int a = 1;
    APPEND(s, int, a);
    APPEND(s, int, 2);
    APPEND(s, int, 3); /* realloc */
    assert(s->base->cap == 4);
    APPEND(s, int, 4);
    APPEND(s, int, 5); /* realloc */
    assert(s->base->cap == 8);
    int sia[] = {1, 2, 3, 4, 5};
    verify_slice(s, sia, 5);

    /* new slice from 0 to s->len with the same base array as s*/
    slice *s1 = SLICE(s, int, 0, s->len);
    assert(!memcmp(s, s1, sizeof(*s))); /* which should be identical to s */
    assert(s1->base->refs == 2); /* should now be 2 refs to the same base */
    slice *s2 = SLICE(s, int, 3, 4); /* another new slice and another ref */
    assert(s2->base->refs == 3 && s2->base == s1->base && s2->base == s->base);
    int s2ia[] = {4};
    verify_slice(s2, s2ia, 1);
    APPEND(s2, int, 5);
    APPEND(s2, int, 6);
    APPEND(s2, int, 7);
    APPEND(s2, int, 8);
    assert(s2->base->cap == 8);
    APPEND(s2, int, 9);
    assert(s2->base->cap == 16);
    int s2ia1[] = {4, 5, 6, 7, 8, 9};
    verify_slice(s2, s2ia1, 6);
    s = DROP(s); /* should just decrease the ref count of the shared base */
    assert(s1->base->refs == 2);

    slice *s3 = SLICE_FROM(s2, int, 1, 4); /* slice with new base array */
    assert(s3->base->refs == 1 && s1->base->refs == 2 && s3->base != s1->base);
    assert(s3->base->cap == 6);
    int s3ia[] = {5, 6, 7};
    verify_slice(s3, s3ia, 3);
    INDEX(s3, int, 1) = 1234; /* INDEX is not bounds checked */
    assert(GET(s3, int, 1) == 1234); /* and GET and PUT are */
    PUT(s3, int, 2, 5678);
    assert(INDEX(s3, int, 2) == 5678);
    APPEND(s3, int, 8);
    assert(s3->base->cap == 6);
    int s3ia1[] = {5, 1234, 5678, 8};
    verify_slice(s3, s3ia1, 4);

    /* when removing from the middle memmove is used */
    REMOVE(s3, int, 1);
    int s3ia2[] = {5, 5678, 8};
    verify_slice(s3, s3ia2, 3);
    /* when removing the head the offset is simply incremented */
    REMOVE(s3, int, 0);
    assert(s3->offset == sizeof(int));
    int s3ia3[] = {5678, 8};
    verify_slice(s3, s3ia3, 2);
    /* when removing from the end the length is simply decremented */
    REMOVE(s3, int, 1);
    int s3ia4[] = {5678};
    verify_slice(s3, s3ia4, 1);

    /* operations on slices affect other slices with the same base array */
    slice *s4 = SLICE(s2, int, 3, 5);
    int s4ia[] = {7, 8};
    verify_slice(s4, s4ia, 2);
    memset(ARR(s4), 0, s4->len * sizeof(int));
    int s4ia1[] = {0, 0};
    verify_slice(s4, s4ia1, 2);
    int s2ia2[] = {4, 5, 6, 0, 0, 9};
    verify_slice(s2, s2ia2, 6);
    s1 = DROP(s1);
    s2 = DROP(s2);
    s3 = DROP(s3);
    s4 = DROP(s4);

    /* MAKE returns a pointer because it's better for composition */
    slice *s5 = MAKE(slice *, 0, 1);
    for (unsigned i = 0; i < 129; i++) {
        APPEND(s5, slice *, MAKE(int, 0, 2));
    }
    assert(s5->base->cap == 256);
    while (s5->len > 0) {
        (void)DROP(INDEX(s5, slice *, 0));
        REMOVE(s5, slice *, 0);
    }
    s5 = DROP(s5);

    int s6ia[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18};
    slice *s6 = SLICE_FROM_N(s6ia, int, 0, 10, 10);
    verify_slice(s6, s6ia, 10);
    /* SLICE_OF is intended to be used when a temporary anonymous slice is
     * wanted and is a C11 generic macro so it can be called on either a slice
     * or an array. CONCAT is a also a C11 generic macro so it can take either
     * a slice or a slice * as its second argument making it easy to compose
     * with SLICE_OF or any other slice macro */
    CONCAT(s6, SLICE_OF(s6, int, 2, 4), int);
    int s6ia2[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 4, 6};
    verify_slice(s6, s6ia2, 12);
    /* FIND returns -1 if it doesn't find a matching element and REMOVE returns
     * immediately when passed -1 so the two can be composed safely. (FIND and
     * REMOVE use long longs because ssize_t is technically not part of the C
     * standard library and I'm trying to keep these headers pure C11) */
    int b = 14;
    int c = 12345;
    REMOVE(s6, int, FIND(s6, int, b));
    REMOVE(s6, int, FIND(s6, int, c));
    int s6ia3[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6};
    verify_slice(s6, s6ia3, 11);

    slice *s7;
    CONCAT((s7 = MAKE(int, 0, 1)), s6, int); /* contrived, obviously */
    verify_slice(s7, s6ia3, 11);
    int ia[] = {1, 3, 5};
    /* SLICE_OF on an array is not concurrency-safe as all slice_of_arr calls
     * share a global base struct */
    CONCAT(s7, SLICE_OF(ia, int, 1, 3), int);
    int s7ia[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6, 3, 5};
    verify_slice(s7, s7ia, 13);
    slice *s8;
    CONCAT(s7, (s8 = SLICE(s7, int, 4, 8)), int); /* again, contrived */
    int s7ia2[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6, 3, 5, 8, 10, 12, 16};
    verify_slice(s7, s7ia2, 17);
    /* SLICE_OF can also be used to create non-anonymous stack allocated (or
     * global) slices but they won't add to the reference count of the base so
     * they must be used very carefully as the base arary may disappear out
     * from under you. DROP must not be called on slices that are created this
     * way. In fact, you probably just shouldn't do this at all and should
     * probably just use SLICE if you want a named slice */
    slice s9 = SLICE_OF(s7, int, 0, 2);
    slice s10 = SLICE_OF(s6, int, 2, 5);
    int s9ia[] = {0, 2};
    int s10ia[] = {4, 6, 8};
    verify_slice(&s9, s9ia, 2);
    verify_slice(&s10, s10ia, 3);
    s6 = DROP(s6);
    s7 = DROP(s7);
    s8 = DROP(s8);

    /*
    slice *s11 = MAKE(int, 0, 512);
    for (unsigned i = 0; i < 1000000000; i++) {
        APPEND(s11, int, i);
    }
    printf("%d\n", INDEX(s11, int, 256));
    printf("%d\n", INDEX(s11, int, 555555));
    printf("%d\n", INDEX(s11, int, 999999999));
    s11 = DROP(s11);
    */

    printf("All tests passed\n");
}
