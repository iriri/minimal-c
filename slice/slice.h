/* Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef SLICE_H
#define SLICE_H
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct base_arr {
    char *arr;
    size_t cap, refs;
} base;

typedef struct slice_hdr {
    base *base;
    char *arr;
    size_t len;
} slice;

#define MAKE(type, len, cap) slice_make(sizeof(type), len, cap)
#define SLICE(hdr, type, head, tail) \
    slice_slice(&hdr, sizeof(type), head, tail, false)
#define SLICE_NEW(hdr, type, head, tail) \
    slice_slice(&hdr, sizeof(type), head, tail, true)
#define DROP(hdr) \
    if (hdr.base->refs == 1) { \
        free(hdr.base->arr); \
        free(hdr.base); \
    } else { \
        hdr.base--; \
    } \
    memset(&hdr, 0, sizeof(hdr))
#define INDEX(hdr, type, index) ((type *)hdr.arr)[index]
#define GET(hdr, type, index) \
    (assert(index < hdr.len), \
     INDEX(hdr, type, index))
#define PUT(hdr, type, index, elt) \
    assert(index < hdr.len); \
    INDEX(hdr, type, index) = elt
#define _MACRO_CONCAT(x, y) x##y
#define MACRO_CONCAT(x, y) _MACRO_CONCAT(x, y)
#define APPEND(hdr, type, elt) \
    type MACRO_CONCAT(_Xtmp, __LINE__) = elt; \
    slice_append(&hdr, sizeof(type), &MACRO_CONCAT(_Xtmp, __LINE__))
#define REMOVE(hdr, type, index) slice_remove(&hdr, sizeof(type), index)

slice
slice_make(size_t elt_size, size_t len, size_t cap) {
    base *b = malloc(sizeof(*b));
    b->arr = calloc(cap, elt_size);
    assert(b->arr && cap > 0 && len <= cap);
    b->cap = cap;
    b->refs = 1;
    return (slice){b, b->arr, len};
}

slice
slice_slice(slice *hdr, size_t elt_size, size_t head, size_t tail, bool new) {
    assert(tail <= hdr->len);
    if (!new) {
        hdr->base->refs++;
        return (slice){hdr->base, hdr->arr + (head * elt_size), tail - head};
    }
    slice s = slice_make(elt_size, tail - head, (tail - head) * 2);
    memcpy(s.arr, hdr->arr + (head * elt_size), s.len * elt_size);
    return s;
}

void
slice_append(slice *hdr, size_t elt_size, void *elt) {
    ptrdiff_t offset = hdr->arr - hdr->base->arr;
    if (hdr->len < hdr->base->cap - (offset / elt_size)) {
        memcpy(hdr->arr + (hdr->len++ * elt_size), elt, elt_size);
        return;
    }
    hdr->base->cap *= 2;
    hdr->base->arr = realloc(hdr->base->arr, hdr->base->cap * elt_size);
    assert(hdr->base->arr);
    hdr->arr = hdr->base->arr + offset;
    memcpy(hdr->arr + (hdr->len++ * elt_size), elt, elt_size);
}

void
slice_remove(slice *hdr, size_t elt_size, size_t index) {
    hdr->len--;
    if (index == hdr->len) {
        return;
    }
    if (index == 0) {
        hdr->arr += elt_size;
        return;
    }
    assert(index <= hdr->len);
    size_t offset = index * elt_size;
    char *dest = hdr->arr + offset;
    memmove(dest, dest + elt_size, (hdr->len * elt_size) - offset);
}
#endif
