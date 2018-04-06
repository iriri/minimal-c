/* test should be compiled with -fsanitize=address */
#include <stdio.h>
#include "slice.h"

#define verify_slice(slice, intarr, _len) __extension__({ \
    __auto_type _Vs = slice; \
    if (_Vs->len != _len) { \
        printf("len mismatch: %lu, %d\n", _Vs->len, _len); \
        assert(0); \
    } \
    for (unsigned i = 0; i < _Vs->len; i++) { \
        if (s_arr(_Vs)[i] != intarr[i]) { \
            printf("%u, %d, %d\n", i, s_arr(_Vs)[i], intarr[i]); \
            assert(0); \
        } \
    } })

/* SLICE_DEF must be called for any slice type that you use. SLICE_DEF_PTR must
 * be called for pointer types. Below we define a slice type that holds ints
 * and a slice type that holds pointers to slices of ints */
SLICE_DEF(int);
SLICE_DEF_PTR(slice(int));

int
main() {
    /* empty slice with initial capacity 2 */
    slice(int) *s = s_make(int, 0, 2);
    int a = 1;
    s_append(s, a);
    s_append(s, 2);
    s_append(s, 3); /* realloc */
    assert(s->base->cap == 4);
    s_append(s, 4);
    s_append(s, 5); /* realloc */
    assert(s->base->cap == 8);
    int sia[] = {1, 2, 3, 4, 5};
    verify_slice(s, sia, 5);

    /* new slice from 0 to s->len with the same base array as s*/
    slice(int) *s1 = s_slice(s, 0, s->len);
    assert(!memcmp(s, s1, sizeof(*s))); /* which should be identical to s */
    assert(s1->base->refs == 2); /* should now be 2 refs to the same base */
    slice(int) *s2 = s_slice(s, 3, 4); /* another new slice and another ref */
    assert(s2->base->refs == 3 && s2->base == s1->base && s2->base == s->base);
    int s2ia[] = {4};
    verify_slice(s2, s2ia, 1);
    s_append(s2, 5);
    s_append(s2, 6);
    s_append(s2, 7);
    s_append(s2, 8);
    assert(s2->base->cap == 8);
    s_append(s2, 9);
    assert(s2->base->cap == 16);
    int s2ia1[] = {4, 5, 6, 7, 8, 9};
    verify_slice(s2, s2ia1, 6);
    s = s_drop(s); /* should just decrease the ref count of the shared base */
    assert(s1->base->refs == 2);

    /* A slice with a new base array can be created from an array, such as the
     * base array of another struct, in an operation that consists of making a
     * new slice and then memcpying over the relevant slice of that array */
    slice(int) *s3 = s_slice_from(s_arr_(s2), int, 1, 4);
    assert(s3->base->refs == 1 && s1->base->refs == 2 && s3->base != s1->base);
    assert(s3->base->cap == 6);
    int s3ia[] = {5, 6, 7};
    verify_slice(s3, s3ia, 3);
    /* simply using the base array is not bounds checked */
    s_arr(s3)[1] = 1234;
    assert(s_get(s3, 1) == 1234); /* while s_get and s_put are */
    s_put(s3, 2, 5678);
    assert(s_arr(s3)[2] == 5678);
    s_append(s3, 8);
    assert(s3->base->cap == 6);
    int s3ia1[] = {5, 1234, 5678, 8};
    verify_slice(s3, s3ia1, 4);

    /* when removing from the middle memmove is used */
    s_remove(s3, 1);
    int s3ia2[] = {5, 5678, 8};
    verify_slice(s3, s3ia2, 3);
    /* when removing the head the offset is simply incremented */
    s_remove(s3, 0);
    assert(s3->offset == 1);
    int s3ia3[] = {5678, 8};
    verify_slice(s3, s3ia3, 2);
    /* when removing from the end the length is simply decremented */
    s_remove(s3, 1);
    int s3ia4[] = {5678};
    verify_slice(s3, s3ia4, 1);

    /* operations on slices affect other slices with the same base array */
    slice(int) *s4 = s_slice(s2, 3, 5);
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

    /* Pointer types must be wrapped in PTR_OF (and the * must be s_dropped) when
     * declaring a new slice or passing a type to the slice creation macros
     * that take a type as an argument. Otherwise storing pointers is just as
     * simple as storing any other type */
    slice(PTR_OF(slice(int))) *s5 = s_make(PTR_OF(slice(int)), 0, 1);
    for (int i = 0; i < 129; i++) {
        s_append(s5, s_make(int, 0, 2));
    }
    assert(s5->base->cap == 256);
    while (s5->len > 0) {
        (void)s_drop(s_arr(s5)[0]);
        s_remove(s5, 0);
    }
    s5 = s_drop(s5);

    int s6ia[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18};
    slice(int) *s6 = s_slice_from_n(s6ia, int, 0, 10, 10);
    verify_slice(s6, s6ia, 10);
    /* s_slice_of is intended to be used when a temporary anonymous slice is
     * wanted and returns a slice struct, which is what s_concat takes for its
     * second argument. Be sure to dereference the second argument if it is a
     * pointer or the compiler will scream at you */
    s_concat(s6, s_slice_of(s6, 2, 4));
    int s6ia2[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 4, 6};
    verify_slice(s6, s6ia2, 12);
    /* s_find returns -1 if it doesn't find a matching element and s_remove
     * returns immediately when passed -1 so the two can be composed safely */
    int b = 14;
    int c = 12345;
    s_remove(s6, s_find(s6, b));
    s_remove(s6, s_find(s6, c));
    int s6ia3[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6};
    verify_slice(s6, s6ia3, 11);

    slice(int) *s7;
    s_concat((s7 = s_make(int, 0, 1)), *s6); /* contrived, obviously */
    verify_slice(s7, s6ia3, 11);
    int ia[] = {1, 3, 5};

    /* Unfortunately creating a temporary slice of an array is much uglier than
     * creating a temporary slice of a slice */
    slice(int) *stemp;
    s_concat(s7, *(stemp = s_slice_from(ia, int, 1, 3)));
    s_drop(stemp);
    int s7ia[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6, 3, 5};
    verify_slice(s7, s7ia, 13);
    slice(int) *s8;
    s_concat(s7, *(s8 = s_slice(s7, 4, 8))); /* again, contrived */
    int s7ia2[] = {0, 2, 4, 6, 8, 10, 12, 16, 18, 4, 6, 3, 5, 8, 10, 12, 16};
    verify_slice(s7, s7ia2, 17);
    /* s_slice_of can also be used to create non-anonymous stack allocated (or
     * global) slices but they won't add to the reference count of the base so
     * they must be used very carefully as the base arary may disappear out
     * from under you. s_drop must not be called on slices that are created
     * this way. In fact, you probably just shouldn't do this at all and should
     * probably just use SLICE if you want a named slice */
    slice(int) s9 = s_slice_of(s7, 0, 2);
    slice(int) s10 = s_slice_of(s6, 2, 5);
    int s9ia[] = {0, 2};
    int s10ia[] = {4, 6, 8};
    verify_slice(&s9, s9ia, 2);
    verify_slice(&s10, s10ia, 3);
    s6 = s_drop(s6);
    s7 = s_drop(s7);
    s8 = s_drop(s8);

    /*
    slice(int) *s11 = s_make(int, 0, 512);
    for (int i = 0; i < 1000000000; i++) {
        s_append(s11, i);
    }
    printf("%d\n", s_arr(s11)[256]);
    printf("%d\n", s_arr(s11)[555555]);
    printf("%d\n", s_arr(s11)[999999999]);
    s11 = s_drop(s11);
    */

    printf("All tests passed\n");
}
