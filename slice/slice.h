/* Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef SLICE_H
#define SLICE_H
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct base_arr {
    char *arr;
    size_t cap, refs;
} base;

typedef struct slice_hdr {
    base *base;
    size_t offset, len;
} slice;

#define MAKE(type, len, cap) slice_make(sizeof(type), len, cap)
#define DROP(hdr) \
    ((hdr)->base->refs == 1 ? \
         (free((hdr)->base->arr), free((hdr)->base)) : \
         (void)(hdr)->base->refs--, \
     free(hdr), NULL)

#define SLICE_N(hdr, type, head, tail, cap) \
    slice_slice(hdr, sizeof(type), head, tail, cap)
#define SLICE(hdr, type, head, tail) SLICE_N(hdr, type, head, tail, 0)
#define SLICE_NEW(hdr, type, head, tail) \
    SLICE_N(hdr, type, head, tail, 2 * (tail - head))

#define ARR(hdr) ((hdr)->base->arr + (hdr)->offset)
#define INDEX(hdr, type, index) ((type *)ARR(hdr))[index]
#define GET(hdr, type, index) \
    (assert((index) < (hdr)->len), INDEX(hdr, type, index))
#define PUT(hdr, type, index, elt) \
    assert((index) < (hdr)->len); \
    INDEX(hdr, type, index) = elt

#define _MACRO_CONCAT(x, y) x##y
#define MACRO_CONCAT(x, y) _MACRO_CONCAT(x, y)
#define APPEND(hdr, type, elt) \
    type MACRO_CONCAT(_Atmp, __LINE__) = elt; \
    slice_append(hdr, sizeof(type), &MACRO_CONCAT(_Atmp, __LINE__))
#define REMOVE(hdr, type, index) slice_remove(hdr, sizeof(type), index)

inline slice *
slice_make(size_t eltsize, size_t len, size_t cap) {
    base *b = malloc(sizeof(*b));
    *b = (base){calloc(cap, eltsize), cap, 1};
    slice *s = malloc(sizeof(*s));
    assert(cap > 0 && len <= cap && b->arr && s);
    *s = (slice){b, 0, len};
    return s;
}

inline slice *
slice_slice(slice *hdr, size_t eltsize, size_t head, size_t tail, size_t cap) {
    if (cap > 0) {
        assert(tail <= hdr->len && tail - head <= cap);
        slice *s = slice_make(eltsize, tail - head, cap);
        memcpy(s->base->arr, ARR(hdr) + (head * eltsize), s->len * eltsize);
        return s;
    }
    hdr->base->refs++;
    slice *s = malloc(sizeof(*s));
    assert(s && tail <= hdr->len);
    *s = (slice){hdr->base, hdr->offset + (head * eltsize), tail - head};
    return s;
}

inline void
slice_append(slice *hdr, size_t eltsize, void *elt) {
    if (hdr->len < hdr->base->cap - (hdr->offset / eltsize)) {
        memcpy(ARR(hdr) + (hdr->len++ * eltsize), elt, eltsize);
        return;
    }
    hdr->base->cap *= 2;
    hdr->base->arr = realloc(hdr->base->arr, hdr->base->cap * eltsize);
    assert(hdr->base->arr);
    memcpy(ARR(hdr) + (hdr->len++ * eltsize), elt, eltsize);
}

inline void
slice_remove(slice *hdr, size_t eltsize, size_t index) {
    hdr->len--;
    if (index == hdr->len) {
        return;
    }
    if (index == 0) {
        hdr->offset += eltsize;
        return;
    }
    assert(index <= hdr->len);
    char *dest = ARR(hdr) + (index * eltsize);
    memmove(dest, dest + eltsize, (hdr->len - index) * eltsize);
}
#endif
