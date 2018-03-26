/* test should be compiled with -fsanitize=address */
#include <stdio.h>
#include "slice.h"

#define verify_slice(slice, intarr, _len) __extension__({ \
    typeof(slice) _Vs = slice; \
    if (_Vs->len != _len) { \
        printf("len mismatch: %lu, %d\n", _Vs->len, _len); \
        assert(0); \
    } \
    for (unsigned i = 0; i < _Vs->len; i++) { \
        if (ARR(_Vs)[i] != intarr[i]) { \
            printf("%u, %d, %d\n", i, ARR(_Vs)[i], intarr[i]); \
            assert(0); \
        } \
    } })

/* SLICE_DEF must be called for any slice type that you use. SLICE_DEF_POINTER
 * must be called for pointer types. Below we define a slice type that holds
 * ints and a slice type that holds pointers to slices of ints */
SLICE_DEF(int);
SLICE_DEF_PTR(slice(int));

int
main() {
    slice(int) *s = MAKE(int, 0, 2); /* empty slice with initial capacity 2 */
    int a = 1;
    APPEND(s, a);
    APPEND(s, 2);
    APPEND(s, 3); /* realloc */
    assert(s->base->cap == 4);
    APPEND(s, 4);
    APPEND(s, 5); /* realloc */
    assert(s->base->cap == 8);
    int sia[] = {1, 2, 3, 4, 5};
    verify_slice(s, sia, 5);

    /* new slice from 0 to s->len with the same base array as s*/
    slice(int) *s1 = SLICE(s, 0, s->len);
    assert(!memcmp(s, s1, sizeof(*s))); /* which should be identical to s */
    assert(s1->base->refs == 2); /* should now be 2 refs to the same base */
    slice(int) *s2 = SLICE(s, 3, 4); /* another new slice and another ref */
    assert(s2->base->refs == 3 && s2->base == s1->base && s2->base == s->base);
    int s2ia[] = {4};
    verify_slice(s2, s2ia, 1);
    APPEND(s2, 5);
    APPEND(s2, 6);
    APPEND(s2, 7);
    APPEND(s2, 8);
    assert(s2->base->cap == 8);
    APPEND(s2, 9);
    assert(s2->base->cap == 16);
    int s2ia1[] = {4, 5, 6, 7, 8, 9};
    verify_slice(s2, s2ia1, 6);
    s = DROP(s); /* should just decrease the ref count of the shared base */
    assert(s1->base->refs == 2);

    /* A slice with a new base array can be created from an array, such as the
     * base array of another struct, in an operation that consists of making a
     * new slice and then memcpying over the relevant slice of that array */
    slice(int) *s3 = SLICE_FROM(ARR_(s2), int, 1, 4);
    assert(s3->base->refs == 1 && s1->base->refs == 2 && s3->base != s1->base);
    assert(s3->base->cap == 6);
    int s3ia[] = {5, 6, 7};
    verify_slice(s3, s3ia, 3);
    ARR(s3)[1] = 1234; /* simply using the base array is not bounds checked */
    assert(GET(s3, 1) == 1234); /* while GET and PUT are */
    PUT(s3, 2, 5678);
    assert(ARR(s3)[2] == 5678);
    APPEND(s3, 8);
    assert(s3->base->cap == 6);
    int s3ia1[] = {5, 1234, 5678, 8};
    verify_slice(s3, s3ia1, 4);

    /* when removing from the middle memmove is used */
    REMOVE(s3, 1);
    int s3ia2[] = {5, 5678, 8};
    verify_slice(s3, s3ia2, 3);
    /* when removing the head the offset is simply incremented */
    REMOVE(s3, 0);
    assert(s3->offset == 1);
    int s3ia3[] = {5678, 8};
    verify_slice(s3, s3ia3, 2);
    /* when removing from the end the length is simply decremented */
    REMOVE(s3, 1);
    int s3ia4[] = {5678};
    verify_slice(s3, s3ia4, 1);

    /* operations on slices affect other slices with the same base array */
    slice(int) *s4 = SLICE(s2, 3, 5);
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

    /* Pointer types must be wrapped in PTR_OF (and the * must be dropped) when
     * declaring a new slice or passing a type to the slice creation macros
     * that take a type as an argument. Otherwise storing pointers is just as
     * simple as storing any other type */
    slice(PTR_OF(slice(int))) *s5 = MAKE(PTR_OF(slice(int)), 0, 1);
    for (unsigned i = 0; i < 129; i++) {
        APPEND(s5, MAKE(int, 0, 2));
    }
    assert(s5->base->cap == 256);
    while (s5->len > 0) {
        (void)DROP(ARR(s5)[0]);
        REMOVE(s5, 0);
    }
    s5 = DROP(s5);

    int s6ia[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18};
    slice(int) *s6 = SLICE_FROM_N(s6ia, int, 0, 10, 10);
    verify_slice(s6, s6ia, 10);
    /* SLICE_OF is intended to be used when a temporary anonymous slice is
     * wanted and returns a slice struct, which is what CONCAT takes for its
     * second argument. Be sure to dereference the second argument if it is a
     * pointer or the compiler will scream at you */
    CONCAT(s6, SLICE_OF(s6, 2, 4));
    int s6ia2[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 4, 6};
    verify_slice(s6, s6ia2, 12);
    /* FIND returns -1 if it doesn't find a matching element and REMOVE returns
     * immediately when passed -1 so the two can be composed safely */
    int b = 14;
    int c = 12345;
    REMOVE(s6, FIND(s6, b));
    REMOVE(s6, FIND(s6, c));
    int s6ia3[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6};
    verify_slice(s6, s6ia3, 11);

    slice(int) *s7;
    CONCAT((s7 = MAKE(int, 0, 1)), *s6); /* contrived, obviously */
    verify_slice(s7, s6ia3, 11);
    int ia[] = {1, 3, 5};

    /* Unfortunately creating a temporary slice of an array is much uglier than
     * creating a temporary slice of a slice */
    slice(int) *stemp;
    CONCAT(s7, *(stemp = SLICE_FROM(ia, int, 1, 3)));
    DROP(stemp);
    int s7ia[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6, 3, 5};
    verify_slice(s7, s7ia, 13);
    slice(int) *s8;
    CONCAT(s7, *(s8 = SLICE(s7, 4, 8))); /* again, contrived */
    int s7ia2[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6, 3, 5, 8, 10, 12, 16};
    verify_slice(s7, s7ia2, 17);
    /* SLICE_OF can also be used to create non-anonymous stack allocated (or
     * global) slices but they won't add to the reference count of the base so
     * they must be used very carefully as the base arary may disappear out
     * from under you. DROP must not be called on slices that are created this
     * way. In fact, you probably just shouldn't do this at all and should
     * probably just use SLICE if you want a named slice */
    slice(int) s9 = SLICE_OF(s7, 0, 2);
    slice(int) s10 = SLICE_OF(s6, 2, 5);
    int s9ia[] = {0, 2};
    int s10ia[] = {4, 6, 8};
    verify_slice(&s9, s9ia, 2);
    verify_slice(&s10, s10ia, 3);
    s6 = DROP(s6);
    s7 = DROP(s7);
    s8 = DROP(s8);

    /*
    slice(int) *s11 = MAKE(int, 0, 512);
    for (unsigned i = 0; i < 1000000000; i++) {
        APPEND(s11, i);
    }
    printf("%d\n", ARR(s11)[256]);
    printf("%d\n", ARR(s11)[555555]);
    printf("%d\n", ARR(s11)[999999999]);
    s11 = DROP(s11);
    */

    printf("All tests passed\n");
}
