/* Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef SLICE_H
#define SLICE_H
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct slice_base_arr {
    char *arr;
    size_t cap, refs;
} slice_base;

typedef struct slice_hdr {
    slice_base *base;
    size_t offset, len;
} slice;

#define MAKE(type, len, cap) slice_make(sizeof(type), len, cap)
#define DROP(hdr) slice_drop(&(hdr))

#define SLICE(hdr, type, head, tail) slice_slice(hdr, sizeof(type), head, tail)
#define SLICE_FROM_N(T, type, head, tail, cap) _Generic((T), \
    slice *: slice_from, \
    default: _slice_from)(T, sizeof(type), head, tail, cap)
#define SLICE_FROM(arr, type, head, tail) \
    SLICE_FROM_N(arr, type, head, tail, 2 * (tail - head))
#define SLICE_OF(T, type, head, tail) _Generic((T), \
    slice *: slice_of, \
    default: slice_of_arr)(T, sizeof(type), head, tail)

#define ARR(hdr) slice_arr(hdr)
#define INDEX(hdr, type, index) ((type *)ARR(hdr))[index]
#define PUT(hdr, type, index, elt) \
    *(type *)slice_index(hdr, sizeof(type), index) = elt
#define GET(hdr, type, index) *(type *)slice_index(hdr, sizeof(type), index)

#define APPEND(hdr, type, elt) *(type *)slice_append(hdr, sizeof(type)) = elt
#define CONCAT(s1, T, type) _Generic((T), \
    slice *: slice_concat, \
    slice: _slice_concat)(s1, T, sizeof(type))
#define FIND(hdr, type, elt) slice_find(hdr, sizeof(type), &elt)
#define REMOVE(hdr, type, index) slice_remove(hdr, sizeof(type), index)

#define SLICE_EXTERN_DECL \
    extern inline slice *slice_make(size_t, size_t, size_t); \
    extern inline void slice_drop(slice **); \
    extern inline char *slice_arr(slice *); \
    extern inline slice *slice_slice(slice *, size_t, size_t, size_t); \
    extern inline slice *_slice_from(void *, size_t, size_t, size_t, size_t); \
    extern inline slice *slice_from(slice *, size_t, size_t, size_t, size_t); \
    extern inline slice slice_of(slice *, size_t, size_t, size_t); \
    extern inline slice slice_of_arr(void *, size_t, size_t, size_t); \
    extern inline void *slice_index(slice *, size_t, size_t); \
    extern inline void slice_grow(slice *, size_t, size_t); \
    extern inline void *slice_append(slice *, size_t); \
    extern inline void _slice_concat(slice *, slice, size_t); \
    extern inline void slice_concat(slice *, slice *, size_t); \
    extern inline long long slice_find(slice *, size_t, void *); \
    extern inline void slice_remove(slice *, size_t, long long); \
    \
    slice_base _slice_tmp_base

extern slice_base _slice_tmp_base;

inline slice *
slice_make(size_t eltsize, size_t len, size_t cap) {
    slice *s = malloc(sizeof(*s));
    *s = (slice){malloc(sizeof(*s->base)), 0, len};
    *s->base = (slice_base){malloc(cap * eltsize), cap, 1};
    assert(cap > 0 && len <= cap && s->base->arr);
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
    assert(head < tail && tail <= hdr->len && s);
    *s = (slice){hdr->base, hdr->offset + (head * eltsize), tail - head};
    return s;
}

inline slice
slice_of(slice *hdr, size_t eltsize, size_t head, size_t tail) {
    assert(head < tail && tail <= hdr->len);
    return (slice){hdr->base, hdr->offset + (head * eltsize), tail - head};
}

inline slice
slice_of_arr(void *arr, size_t eltsize, size_t head, size_t tail) {
    assert(head < tail);
    _slice_tmp_base.arr = (char *)arr + (head * eltsize);
    return (slice){&_slice_tmp_base, 0, tail - head};
}

inline slice *
_slice_from(void *arr, size_t eltsize, size_t head, size_t tail, size_t cap) {
    assert(head < tail && tail - head <= cap);
    slice *s = slice_make(eltsize, tail - head, cap);
    memcpy(s->base->arr, (char *)arr + (head * eltsize), s->len * eltsize);
    return s;
}

inline slice *
slice_from(slice *hdr, size_t eltsize, size_t head, size_t tail, size_t cap) {
    return _slice_from(ARR(hdr), eltsize, head, tail, cap);
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
_slice_concat(slice *s1, slice s2, size_t eltsize) {
    if (s1->len + s2.len > s1->base->cap - (s1->offset / eltsize)) {
        slice_grow(s1, eltsize,
                   s1->base->cap + (s1->base->cap > s2.base->cap ?
                                        s1->base->cap : s2.base->cap));
    }
    memmove(ARR(s1) + (s1->len * eltsize), ARR(&s2), s2.len * eltsize);
    s1->len += s2.len;
}

inline void
slice_concat(slice *s1, slice *s2, size_t eltsize) {
    _slice_concat(s1, *s2, eltsize);
}

inline long long
slice_find(slice *hdr, size_t eltsize, void *elt) {
    for (size_t i = 0; i < hdr->len; i++) {
        if (memcmp(ARR(hdr) + (i * eltsize), elt, eltsize) == 0) {
            return i;
        }
    }
    return -1;
}

inline void
slice_remove(slice *hdr, size_t eltsize, long long index) {
    if (index < 0) {
        return;
    }
    size_t i = index;
    hdr->len--;
    if (i == 0) {
        hdr->offset += eltsize;
        return;
    }
    assert(i <= hdr->len);
    if (i != hdr->len) {
        memmove(ARR(hdr) + (i * eltsize),
                ARR(hdr) + (i * eltsize) + eltsize,
                (hdr->len - i) * eltsize);
    }
}
#endif
