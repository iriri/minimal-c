/* Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef SLICE_H
#define SLICE_H
#include <assert.h>
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
#define DROP(hdr) slice_drop(&hdr)

#define SLICE(hdr, type, head, tail) slice_slice(hdr, sizeof(type), head, tail)
#define SLICE_OF(arr, type, head, tail) slice_of(arr, sizeof(type), head, tail)
#define SLICE_FROM_N(arr, type, head, tail, cap) \
    slice_from(arr, sizeof(type), head, tail, cap)
#define SLICE_FROM(arr, type, head, tail) \
    SLICE_FROM_N(arr, type, head, tail, 2 * (tail - head))

#define ARR(hdr) slice_arr(hdr)
#define INDEX(hdr, type, index) ((type *)ARR(hdr))[index]
#define PUT(hdr, type, index, elt) \
    *(type *)slice_index(hdr, sizeof(type), index) = elt
#define GET(hdr, type, index) *(type *)slice_index(hdr, sizeof(type), index)

#define APPEND(hdr, type, elt) *(type *)slice_append(hdr, sizeof(type)) = elt
#define CONCAT(s1, s2, type) slice_concat(s1, s2, sizeof(type))

#define FIND(hdr, type, elt) slice_find(hdr, sizeof(type), &elt)
#define REMOVE(hdr, type, index) slice_remove(hdr, sizeof(type), index)

#define SLICE_EXTERN_DECL \
    extern inline slice *slice_make(size_t, size_t, size_t); \
    extern inline void slice_drop(slice **); \
    extern inline char *slice_arr(slice *); \
    extern inline slice *slice_slice(slice *, size_t, size_t, size_t); \
    extern inline slice *slice_of(void *, size_t, size_t, size_t); \
    extern inline slice *slice_from(void *, size_t, size_t, size_t, size_t); \
    extern inline void *slice_index(slice *, size_t, size_t); \
    extern inline void slice_grow(slice *, size_t, size_t); \
    extern inline void *slice_append(slice *, size_t); \
    extern inline void slice_concat(slice *, slice *, size_t); \
    extern inline ssize_t slice_find(slice *, size_t, void *); \
    extern inline void slice_remove(slice *, size_t, ssize_t); \
    \
    base _slice_b; \
    slice _slice_s

extern base _slice_b;
extern slice _slice_s;

inline slice *
slice_make(size_t eltsize, size_t len, size_t cap) {
    base *b = malloc(sizeof(*b));
    *b = (base){calloc(cap, eltsize), cap, 1};
    slice *s = malloc(sizeof(*s));
    assert(cap > 0 && len <= cap && b->arr && s);
    *s = (slice){b, 0, len};
    return s;
}

inline void
slice_drop(slice **hdr) {
    if (--(*hdr)->base->refs == 0) {
        free((*hdr)->base->arr);
        free((*hdr)->base);
    }
    free(*hdr);
    *hdr = NULL;
}

inline char *
slice_arr(slice *hdr) {
    return hdr->base->arr + hdr->offset;
}

inline slice *
slice_slice(slice *hdr, size_t eltsize, size_t head, size_t tail) {
    hdr->base->refs++;
    slice *s = malloc(sizeof(*s));
    assert(s && tail <= hdr->len);
    *s = (slice){hdr->base, hdr->offset + (head * eltsize), tail - head};
    return s;
}

inline slice *
slice_of(void *arr, size_t eltsize, size_t head, size_t tail) {
    _slice_b = (base){(char *)arr + (head * eltsize), 0, 0};
    _slice_s = (slice){&_slice_b, 0, tail - head};
    return &_slice_s;
}

inline slice *
slice_from(void *arr, size_t eltsize, size_t head, size_t tail, size_t cap) {
    assert(head < tail && tail - head <= cap);
    slice *s = slice_make(eltsize, tail - head, cap);
    memcpy(s->base->arr, (char *)arr + (head * eltsize), s->len * eltsize);
    return s;
}

inline void *
slice_index(slice *hdr, size_t eltsize, size_t index) {
    assert(index < hdr->len);
    return ARR(hdr) + (index * eltsize);
}

inline void
slice_grow(slice *hdr, size_t eltsize, size_t cap) {
    hdr->base->arr = realloc(hdr->base->arr, (hdr->base->cap = cap) * eltsize);
    assert(hdr->base->arr);
}

inline void *
slice_append(slice *hdr, size_t eltsize) {
    if (hdr->len == hdr->base->cap - (hdr->offset / eltsize)) {
        slice_grow(hdr, eltsize, 2 * hdr->base->cap);
    }
    return ARR(hdr) + (hdr->len++ * eltsize);
}

inline void
slice_concat(slice *s1, slice *s2, size_t eltsize) {
    if (s1->len + s2->len > s1->base->cap - (s1->offset / eltsize)) {
        slice_grow(s1, eltsize,
                   s1->base->cap > s2->base->cap ?
                       s1->base->cap :
                       s2->base->cap);
    }
    memmove(ARR(s1) + (s1->len * eltsize), ARR(s2), s2-> len * eltsize);
    s1->len += s2->len;
}

inline ssize_t
slice_find(slice *hdr, size_t eltsize, void *elt) {
    for (size_t i = 0; i < hdr->len; i++) {
        if (memcmp(ARR(hdr) + (i * eltsize), elt, eltsize) == 0) {
            return i;
        }
    }
    return -1;
}

inline void
slice_remove(slice *hdr, size_t eltsize, ssize_t index) {
    if (index < 0) {
        return;
    }
    size_t i = index;
    hdr->len--;
    if (i == hdr->len) {
        return;
    }
    if (i == 0) {
        hdr->offset += eltsize;
        return;
    }
    assert(i <= hdr->len);
    memmove(ARR(hdr) + (i * eltsize), ARR(hdr) + (i * eltsize) + eltsize,
            (hdr->len - i) * eltsize);
}
#endif
