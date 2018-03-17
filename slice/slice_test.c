/* Test should be compiled with -fsanitize=address */
#include <stdio.h>
#include "slice.h"

/* C99 inline rules are a little weird. When a function is defined as "inline"
 * in a header file, one "extern" declaration in one (and only one) translation
 * unit is needed to actually generate the non-inline version of that function.
 * Defining functions this way in a single header library, as opposed to with
 * "static inline", requires this small bit of extra work on the part of the
 * user but avoids code duplication when the header is included in multiple
 * translation units
 */
extern inline slice *slice_make(size_t, size_t, size_t);
extern inline slice *slice_slice(slice *, size_t, size_t, size_t, size_t);
extern inline void slice_append(slice *, size_t, void *);
extern inline void slice_remove(slice *, size_t, size_t);

void
verify_slice(slice *s, int *ia, size_t len) {
    assert(s->len == len);
    for (unsigned i = 0; i < s->len; i++) {
        if (INDEX(s, int, i) != ia[i]) {
            printf("%u, %d, %d\n", i, INDEX(s, int, i), ia[i]);
            assert(false);
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

    slice *s3 = SLICE_NEW(s2, int, 1, 4); /* new slice with new base array */
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

    /* MAKE returns a pointer now because it's better for composition */
    slice *s5;
    APPEND((s5 = MAKE(int, 0, 2)), int, 1);
    int s5ia[] = {1};
    verify_slice(s5, s5ia, 1);
    slice *s6 = MAKE(slice *, 0, 1);
    for (unsigned i = 0; i < 129; i++) {
        APPEND(s6, slice *, MAKE(int, 0, 2));
    }
    assert(s6->base->cap == 256);
    while (s6->len > 0) {
        (void)DROP(INDEX(s6, slice *, 0));
        REMOVE(s6, slice *, 0);
    }

    s5 = DROP(s5);
    s6 = DROP(s6);
    printf("All tests passed\n");
}
