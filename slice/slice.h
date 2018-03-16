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
    if ((hdr).base->refs == 1) { \
        free((hdr).base->arr); \
        free((hdr).base); \
    } else { \
        (hdr).base->refs--; \
    } \
    memset(&(hdr), 0, sizeof(hdr))

#define SLICE(hdr, type, head, tail) \
    slice_slice(&(hdr), sizeof(type), head, tail, false)
#define SLICE_NEW(hdr, type, head, tail) \
    slice_slice(&(hdr), sizeof(type), head, tail, true)

#define ARR(hdr) ((hdr).base->arr + (hdr).offset)
#define INDEX(hdr, type, index) ((type *)ARR(hdr))[index]
#define GET(hdr, type, index) \
    (assert((index) < (hdr).len), INDEX(hdr, type, index))
#define PUT(hdr, type, index, elt) \
    assert((index) < (hdr).len); \
    INDEX(hdr, type, index) = elt

#define _MACRO_CONCAT(x, y) x##y
#define MACRO_CONCAT(x, y) _MACRO_CONCAT(x, y)
#define APPEND(hdr, type, elt) \
    type MACRO_CONCAT(_Xtmp, __LINE__) = elt; \
    slice_append(&(hdr), sizeof(type), &MACRO_CONCAT(_Xtmp, __LINE__))
#define REMOVE(hdr, type, index) slice_remove(&(hdr), sizeof(type), index)

slice
slice_make(size_t eltsize, size_t len, size_t cap) {
    base *b = malloc(sizeof(*b));
    b->arr = calloc(cap, eltsize);
    assert(b->arr && cap > 0 && len <= cap);
    b->cap = cap;
    b->refs = 1;
    return (slice){b, 0, len};
}

slice
slice_slice(slice *hdr, size_t eltsize, size_t head, size_t tail, bool new) {
    assert(tail <= hdr->len);
    if (!new) {
        hdr->base->refs++;
        return (slice){hdr->base, hdr->offset + (head * eltsize), tail - head};
    }
    slice s = slice_make(eltsize, tail - head, (tail - head) * 2);
    memcpy(s.base->arr, ARR(*hdr) + (head * eltsize), s.len * eltsize);
    return s;
}

void
slice_append(slice *hdr, size_t eltsize, void *elt) {
    if (hdr->len < hdr->base->cap - (hdr->offset / eltsize)) {
        memcpy(ARR(*hdr) + (hdr->len++ * eltsize), elt, eltsize);
        return;
    }
    hdr->base->cap *= 2;
    hdr->base->arr = realloc(hdr->base->arr, hdr->base->cap * eltsize);
    assert(hdr->base->arr);
    memcpy(ARR(*hdr) + (hdr->len++ * eltsize), elt, eltsize);
}

void
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
    size_t offset = index * eltsize;
    char *dest = ARR(*hdr) + offset;
    memmove(dest, dest + eltsize, (hdr->len * eltsize) - offset);
}
#endif
