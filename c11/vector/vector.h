/* Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef VECTOR_H
#define VECTOR_H
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct vector_hdr {
    char *arr;
    size_t len, cap;
} vector;

#define vec_alloc(type, len, cap) vector_alloc(sizeof(type), len, cap)
#define vec_free(v) vector_free(&v)

#define vec_arr(v, type) ((type *)v.arr)
#define vec_put(v, type, index, elt) \
    *(type *)vector_index(v, sizeof(type), index) = elt
#define vec_get(v, type, index) *(type *)vector_index(v, sizeof(type), index)

#define vec_append(v, type, elt) *(type *)vector_append(&v, sizeof(type)) = elt
#define vec_concat(v1, type, v2) vector_concat(&v1, sizeof(type), v2)
#define vec_find(v, type, elt) vector_find(v, sizeof(type), &elt)
#define vec_remove(v, type, index) vector_remove(&v, sizeof(type), index)

#define VECTOR_EXTERN_DECL \
    extern inline vector vector_alloc(size_t, size_t, size_t); \
    extern inline void vector_free(vector *); \
    extern inline void *vector_index(vector, size_t, size_t); \
    extern inline void *vector_append(vector *, size_t); \
    extern inline void vector_concat(vector *, size_t, vector); \
    extern inline ssize_t vector_find(vector, size_t, void *); \
    extern inline void vector_remove(vector *, size_t, ssize_t)

inline vector
vector_alloc(size_t eltsize, size_t len, size_t cap) {
    assert(cap > 0 && len <= cap);
    return (vector){malloc(cap * eltsize), len, cap};
}

inline void
vector_free(vector *v) {
    free(v->arr);
    v->arr = NULL;
}

inline void *
vector_index(vector v, size_t eltsize, size_t index) {
    assert(index < v.len);
    return v.arr + (index * eltsize);
}

inline void *
vector_append(vector *v, size_t eltsize) {
    if (v->len == v->cap) {
        assert((v->arr = realloc(v->arr, (v->cap *= 2) * eltsize)));
    }
    return v->arr + (v->len++ * eltsize);
}

inline void
vector_concat(vector *v1, size_t eltsize, vector v2) {
    if (v1->len + v2.len > v1->cap) {
        assert(
            (v1->arr = realloc(
                v1->arr, (v1->cap += v1->cap > v2.cap ? v1->cap : v2.cap))));
    }
    memmove(v1->arr + (v1->len * eltsize), v2.arr, v2.len * eltsize);
    v1->len += v2.len;
}

inline ssize_t
vector_find(vector v, size_t eltsize, void *elt) {
    for (size_t i = 0; i < v.len; i++) {
        if (memcmp(v.arr + (i * eltsize), elt, eltsize) == 0) {
            return i;
        }
    }
    return -1;
}

void
vector_remove(vector *v, size_t eltsize, ssize_t index) {
    size_t i = index;
    if (index < 0) {
        return;
    }
    v->len--;
    assert(i <= v->len);
    if (i != v->len) {
        memmove(
            v->arr + (i * eltsize),
            v->arr + ((i + 1) * eltsize),
            (v->len - i) * eltsize);
    }
}
#endif
