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
 * by by s_slice_of when it is called on an array */
SLICE_EXTERN_DECL;

void
verify_slice(slice *s, int *ia, size_t len) {
    if (s->len != len) {
        printf("len mismatch: %lu, %lu\n", s->len, len);
        assert(0);
    }
    for (unsigned i = 0; i < s->len; i++) {
        if (s_index(s, int, i) != ia[i]) {
            printf("%u, %d, %d\n", i, s_index(s, int, i), ia[i]);
            assert(0);
        }
    }
}

int
main() {
    slice *s = s_make(int, 0, 2); /* empty slice with initial capacity 2 */
    int a = 1;
    s_append(s, int, a);
    s_append(s, int, 2);
    s_append(s, int, 3); /* realloc */
    assert(s->base->cap == 4);
    s_append(s, int, 4);
    s_append(s, int, 5); /* realloc */
    assert(s->base->cap == 8);
    int sia[] = {1, 2, 3, 4, 5};
    verify_slice(s, sia, 5);

    /* new slice from 0 to s->len with the same base array as s*/
    slice *s1 = s_slice(s, int, 0, s->len);
    assert(!memcmp(s, s1, sizeof(*s))); /* which should be identical to s */
    assert(s1->base->refc == 2); /* should now be 2 refc to the same base */
    slice *s2 = s_slice(s, int, 3, 4); /* another new slice and another ref */
    assert(s2->base->refc == 3 && s2->base == s1->base && s2->base == s->base);
    int s2ia[] = {4};
    verify_slice(s2, s2ia, 1);
    s_append(s2, int, 5);
    s_append(s2, int, 6);
    s_append(s2, int, 7);
    s_append(s2, int, 8);
    assert(s2->base->cap == 8);
    s_append(s2, int, 9);
    assert(s2->base->cap == 16);
    int s2ia1[] = {4, 5, 6, 7, 8, 9};
    verify_slice(s2, s2ia1, 6);
    s = s_drop(s); /* should just decrease the ref count of the shared base */
    assert(s1->base->refc == 2);

    slice *s3 = s_slice_from(s2, int, 1, 4); /* slice with new base array */
    assert(s3->base->refc == 1 && s1->base->refc == 2 && s3->base != s1->base);
    assert(s3->base->cap == 6);
    int s3ia[] = {5, 6, 7};
    verify_slice(s3, s3ia, 3);
    s_index(s3, int, 1) = 1234; /* s_index is not bounds checked */
    assert(s_get(s3, int, 1) == 1234); /* and s_get and s_put are */
    s_put(s3, int, 2, 5678);
    assert(s_index(s3, int, 2) == 5678);
    s_append(s3, int, 8);
    assert(s3->base->cap == 6);
    int s3ia1[] = {5, 1234, 5678, 8};
    verify_slice(s3, s3ia1, 4);

    /* when removing from the middle memmove is used */
    s_remove(s3, int, 1);
    int s3ia2[] = {5, 5678, 8};
    verify_slice(s3, s3ia2, 3);
    s_remove(s3, int, 0);
    int s3ia3[] = {5678, 8};
    verify_slice(s3, s3ia3, 2);
    /* when removing from the end the length is simply decremented */
    s_remove(s3, int, 1);
    int s3ia4[] = {5678};
    verify_slice(s3, s3ia4, 1);

    /* operations on slices affect other slices with the same base array */
    slice *s4 = s_slice(s2, int, 3, 5);
    int s4ia[] = {7, 8};
    verify_slice(s4, s4ia, 2);
    memset(s_arr(s4), 0, s4->len * sizeof(int));
    int s4ia1[] = {0, 0};
    verify_slice(s4, s4ia1, 2);
    int s2ia2[] = {4, 5, 6, 0, 0, 9};
    verify_slice(s2, s2ia2, 6);
    s1 = s_drop(s1);
    s2 = s_drop(s2);
    s3 = s_drop(s3);
    s4 = s_drop(s4);

    /* s_make returns a pointer because it's better for composition */
    slice *s5 = s_make(slice *, 0, 1);
    for (int i = 0; i < 129; i++) {
        s_append(s5, slice *, s_make(int, 0, 2));
    }
    assert(s5->base->cap == 256);
    while (s5->len > 0) {
        (void)s_drop(s_index(s5, slice *, 0));
        s_remove(s5, slice *, 0);
    }
    s5 = s_drop(s5);

    int s6ia[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18};
    slice *s6 = s_slice_from_n(s6ia, int, 0, 10, 10);
    verify_slice(s6, s6ia, 10);
    /* s_slice_of is intended to be used when a temporary anonymous slice is
     * wanted and is a C11 generic macro so it can be called on either a slice
     * or an array. s_concat is a also a C11 generic macro so it can take
     * either a slice or a slice * as its second argument making it easy to
     * compose with s_slice_of or any other slice macro */
    s_concat(s6, int, s_slice_of(s6, int, 2, 4));
    int s6ia2[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 4, 6};
    verify_slice(s6, s6ia2, 12);
    /* s_find returns -1 if it doesn't find a matching element and s_remove
     * returns immediately when passed -1 so the two can be composed safely.
     * (s_find and s_remove use long longs because ssize_t is technically not
     * part of the C standard library and I'm trying to keep these headers pure
     * C11) */
    int b = 14;
    int c = 12345;
    s_remove(s6, int, s_find(s6, int, b));
    s_remove(s6, int, s_find(s6, int, c));
    int s6ia3[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6};
    verify_slice(s6, s6ia3, 11);

    slice *s7;
    s_concat((s7 = s_make(int, 0, 1)), int, s6); /* contrived, obviously */
    verify_slice(s7, s6ia3, 11);
    int ia[] = {1, 3, 5};
    /* s_slice_of on an array is not concurrency-safe as all slice_of_arr calls
     * share a global base struct */
    s_concat(s7, int, s_slice_of(ia, int, 1, 3));
    int s7ia[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6, 3, 5};
    verify_slice(s7, s7ia, 13);
    slice *s8;
    s_concat(s7, int, (s8 = s_slice(s7, int, 4, 8))); /* again, contrived */
    int s7ia2[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6, 3, 5, 8, 10, 12, 16};
    verify_slice(s7, s7ia2, 17);
    /* s_slice_of can also be used to create non-anonymous stack allocated (or
     * global) slices but they won't add to the reference count of the base so
     * they must be used very carefully as the base arary may disappear out
     * from under you. s_drop must not be called on slices that are created
     * this way. In fact, you probably just shouldn't do this at all and should
     * probably just use s_slice if you want a named slice */
    slice s9 = s_slice_of(s7, int, 0, 2);
    slice s10 = s_slice_of(s6, int, 2, 5);
    int s9ia[] = {0, 2};
    int s10ia[] = {4, 6, 8};
    verify_slice(&s9, s9ia, 2);
    verify_slice(&s10, s10ia, 3);
    s6 = s_drop(s6);
    s7 = s_drop(s7);
    s8 = s_drop(s8);

    /*
    slice *s11 = s_make(int, 0, 512);
    for (int i = 0; i < 1000000000; i++) {
        s_append(s11, int, i);
    }
    printf("%d\n", s_index(s11, int, 256));
    printf("%d\n", s_index(s11, int, 555555));
    printf("%d\n", s_index(s11, int, 999999999));
    s11 = s_drop(s11);
    */

    printf("All tests passed\n");
}
