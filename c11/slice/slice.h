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

#define s_make(type, len, cap) slice_make(sizeof(type), len, cap)
#define s_drop(hdr) slice_drop(hdr)

#define s_slice(hdr, type, head, tail) \
    slice_slice(hdr, sizeof(type), head, tail)
#define s_slice_from_n(T, type, head, tail, cap) _Generic((T), \
    slice *: slice_slice_from, \
    default: slice_slice_from_)(T, sizeof(type), head, tail, cap)
#define s_slice_from(arr, type, head, tail) \
    s_slice_from_n(arr, type, head, tail, 2 * (tail - head))
#define s_slice_of(T, type, head, tail) _Generic((T), \
    slice *: slice_slice_of, \
    default: slice_slice_of_arr)(T, sizeof(type), head, tail)

#define s_arr(hdr) slice_arr(hdr)
#define s_index(hdr, type, index) ((type *)slice_arr(hdr))[index]
#define s_put(hdr, type, index, elt) \
    *(type *)slice_index_safe(hdr, sizeof(type), index) = elt
#define s_get(hdr, type, index) \
    *(type *)slice_index_safe(hdr, sizeof(type), index)

#define s_append(hdr, type, elt) *(type *)slice_append(hdr, sizeof(type)) = elt
#define s_concat(s1, T, type) _Generic((T), \
    slice *: slice_concat, \
    slice: slice_concat_)(s1, T, sizeof(type))
#define s_find(hdr, type, elt) slice_find(hdr, sizeof(type), &elt)
#define s_remove(hdr, type, index) slice_remove(hdr, sizeof(type), index)

#define SLICE_EXTERN_DECL \
    extern inline slice *slice_make(size_t, size_t, size_t); \
    extern inline void *slice_drop(slice *); \
    extern inline char *slice_arr(slice *); \
    extern inline slice *slice_slice(slice *, size_t, size_t, size_t); \
    extern inline slice *slice_slice_from_(void *, size_t, size_t, size_t, \
                                           size_t); \
    extern inline slice *slice_slice_from(slice *, size_t, size_t, size_t, \
                                          size_t); \
    extern inline slice slice_slice_of(slice *, size_t, size_t, size_t); \
    extern inline slice slice_slice_of_arr(void *, size_t, size_t, size_t); \
    extern inline void *slice_index_safe(slice *, size_t, size_t); \
    extern inline void slice_grow(slice *, size_t, size_t); \
    extern inline void *slice_append(slice *, size_t); \
    extern inline void slice_concat_(slice *, slice, size_t); \
    extern inline void slice_concat(slice *, slice *, size_t); \
    extern inline long long slice_find(slice *, size_t, void *); \
    extern inline void slice_remove(slice *, size_t, long long); \
    \
    slice_base slice_tmp_base_

extern slice_base slice_tmp_base_;

inline slice *
slice_make(size_t eltsize, size_t len, size_t cap) {
    slice *s = malloc(sizeof(*s));
    *s = (slice){malloc(sizeof(*s->base)), 0, len};
    *s->base = (slice_base){malloc(cap * eltsize), cap, 1};
    assert(cap > 0 && len <= cap && s->base->arr);
    return s;
}

inline void *
slice_drop(slice *hdr) {
    if (--hdr->base->refs == 0) {
        free(hdr->base->arr);
        free(hdr->base);
    }
    free(hdr);
    return NULL;
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
slice_slice_of(slice *hdr, size_t eltsize, size_t head, size_t tail) {
    assert(head < tail && tail <= hdr->len);
    return (slice){hdr->base, hdr->offset + (head * eltsize), tail - head};
}

inline slice
slice_slice_of_arr(void *arr, size_t eltsize, size_t head, size_t tail) {
    assert(head < tail);
    slice_tmp_base_.arr = (char *)arr + (head * eltsize);
    return (slice){&slice_tmp_base_, 0, tail - head};
}

inline slice *
slice_slice_from_(void *arr, size_t eltsize, size_t head, size_t tail, size_t cap) {
    assert(head < tail && tail - head <= cap);
    slice *s = slice_make(eltsize, tail - head, cap);
    memcpy(s->base->arr, (char *)arr + (head * eltsize), s->len * eltsize);
    return s;
}

inline slice *
slice_slice_from(slice *hdr, size_t eltsize, size_t head, size_t tail, size_t cap) {
    return slice_slice_from_(slice_arr(hdr), eltsize, head, tail, cap);
}

inline void *
slice_index_safe(slice *hdr, size_t eltsize, size_t index) {
    assert(index < hdr->len);
    return slice_arr(hdr) + (index * eltsize);
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
    return slice_arr(hdr) + (hdr->len++ * eltsize);
}

inline void
slice_concat_(slice *s1, slice s2, size_t eltsize) {
    if (s1->len + s2.len > s1->base->cap - (s1->offset / eltsize)) {
        slice_grow(s1, eltsize,
                   s1->base->cap + (s1->base->cap > s2.base->cap ?
                                        s1->base->cap : s2.base->cap));
    }
    memmove(slice_arr(s1) + (s1->len * eltsize), slice_arr(&s2),
            s2.len * eltsize);
    s1->len += s2.len;
}

inline void
slice_concat(slice *s1, slice *s2, size_t eltsize) {
    slice_concat_(s1, *s2, eltsize);
}

inline long long
slice_find(slice *hdr, size_t eltsize, void *elt) {
    for (size_t i = 0; i < hdr->len; i++) {
        if (memcmp(slice_arr(hdr) + (i * eltsize), elt, eltsize) == 0) {
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
        memmove(slice_arr(hdr) + (i * eltsize),
                slice_arr(hdr) + (i * eltsize) + eltsize,
                (hdr->len - i) * eltsize);
    }
}
#endif
