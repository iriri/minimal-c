/* vector.h v0.0.0
 * Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef VECTOR_H
#define VECTOR_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------- Interface ------------------------------- */
#define VECTOR_H_VERSION 0l // 0.0.0
#define VECTOR_H_STD

typedef struct vector vector;

/* Exported "functions" */
#define vec_make(T, len, cap) vector_make(sizeof(T), len, cap)
#define vec_drop(v) vector_drop(v)
#define vec_shrink(v) vector_shrink(v)
#define vec_trim(v) vector_trim(v)

#define vec_len(v) (v->len)
#define vec_arr(v, T) ((T *)v->arr)
#define vec_index(v, T, index) (*(T *)vector_index(v, sizeof(T), index))

#define vec_push(v, T, elt) ((void)(*(T *)vector_push(v, sizeof(T)) = elt))

/* TODO Is there a way to do this "type check" at compile-time? `static_assert`
 * is a statement, not an expression, so it can't be used with the comma
 * operator. Is this weak of a "type check" even worth doing? */
#define vec_peek(v, T, elt) \
    ( \
        vec_assert_(sizeof(T) == sizeof(elt)), \
        vector_peek(v, sizeof(T), &elt, false) \
    )
#define vec_pop(v, T, elt) \
    ( \
        vec_assert_(sizeof(T) == sizeof(elt)), \
        vector_peek(v, sizeof(T), &elt, true) \
    )

#define vec_concat(v, v1) vector_concat(v, v1)
#define vec_find(v, elt) vector_find(v, sizeof(elt), &elt)
#define vec_remove(v, index) vector_remove(v, index)

/* These declarations must be present in exactly one compilation unit. */
#define VECTOR_EXTERN_DECL \
    extern inline void vector_assert_( \
        const char *, unsigned, const char *) __attribute__((noreturn)); \
    extern inline vector *vector_make(size_t, size_t, size_t); \
    extern inline vector *vector_drop(vector *); \
    extern inline void vector_shrink(vector *); \
    extern inline void vector_trim(vector *); \
    extern inline void *vector_index(vector *, size_t, size_t); \
    extern inline void *vector_push(vector *, size_t); \
    extern inline bool vector_peek(vector *, size_t, void *, bool); \
    extern inline void vector_concat(vector *, vector *); \
    extern inline size_t vector_find(vector *, size_t, void *); \
    extern inline void vector_remove(vector *, size_t)

/* ---------------------------- Implementation ---------------------------- */
struct vector {
    char *arr;
    size_t eltsize, len, cap;
};

/* `vec_assert_` never becomes a noop, even when `NDEBUG` is set. */
#define vec_assert_(pred) \
    (__builtin_expect(!(pred), 0) ? \
        vector_assert_(__FILE__, __LINE__, #pred) : (void)0)

__attribute__((noreturn)) inline void
vector_assert_(const char *file, unsigned line, const char *pred) {
    fprintf(stderr, "Failed assertion: %s, %u, %s\n", file, line, pred);
    abort();
}

inline vector *
vector_make(size_t eltsize, size_t len, size_t cap) {
    vec_assert_(
        eltsize <= SIZE_MAX / 8 && // Ugh lol; see `vector_push`
        cap <= SIZE_MAX / eltsize &&
        len <= cap);
    vector *v = malloc(sizeof(*v));
    vec_assert_(v);
    *v = (vector){malloc(cap * eltsize), eltsize, len, cap};
    vec_assert_(v->arr || cap == 0);
    return v;
}

inline vector *
vector_drop(vector *v) {
    free(v->arr);
    free(v);
    return NULL;
}

inline void
vector_shrink(vector *v) {
    if (v->len * 4 > v->cap) {
        return;
    }
    vec_assert_(
        (v->arr = realloc(v->arr, (v->cap = 2 * v->len) * v->eltsize)) ||
        v->len == 0);
}

inline void
vector_trim(vector *v) {
    if (v->len == v->cap) {
        return;
    }
    vec_assert_(
        (v->arr = realloc(v->arr, (v->cap = v->len) * v->eltsize)) ||
        v->len == 0);
}

inline void *
vector_index(vector *v, size_t eltsize, size_t index) {
    vec_assert_(eltsize == v->eltsize && index < v->len);
    return v->arr + (index * eltsize);
}

inline void *
vector_push(vector *v, size_t eltsize) {
    vec_assert_(eltsize == v->eltsize);
    if (v->len == v->cap) {
        if (v->cap == 0) {
            v->cap = 8;
        } else {
            vec_assert_(v->cap <= SIZE_MAX - v->cap &&
                (v->cap *= 2) <= SIZE_MAX / eltsize);
        }
        vec_assert_((v->arr = realloc(v->arr, v->cap * eltsize)));
    }
    return v->arr + (v->len++ * eltsize);
}

inline bool
vector_peek(vector *v, size_t eltsize, void *elt, bool pop) {
    vec_assert_(eltsize == v->eltsize);
    if (v->len == 0) {
        return false;
    }
    memcpy(elt, v->arr + ((pop ? --v->len : v->len - 1) * eltsize), eltsize);
    return true;
}

inline void
vector_concat(vector *v, vector *v1) {
    vec_assert_(v->eltsize == v1->eltsize);
    if (v->len + v1->len > v->cap) {
        size_t g = v->cap > v1->cap ? v->cap : v1->cap;
        vec_assert_(v->cap <= SIZE_MAX - g &&
            (v->cap += g) <= SIZE_MAX / v->eltsize);
        vec_assert_((v->arr = realloc(v->arr, v->cap * v->eltsize)));
    }
    memmove(v->arr + (v->len * v->eltsize), v1->arr, v1->len * v1->eltsize);
    v->len += v1->len;
}

inline size_t
vector_find(vector *v, size_t eltsize, void *elt) {
    vec_assert_(eltsize == v->eltsize);
    for (size_t i = 0; i < v->len; i++) {
        if (memcmp(v->arr + (i * eltsize), elt, eltsize) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

inline void
vector_remove(vector *v, size_t index) {
    if (index == SIZE_MAX) {
        return;
    }
    v->len--;
    vec_assert_(index <= v->len);
    if (index != v->len) {
        memmove(
            v->arr + (index * v->eltsize),
            v->arr + ((index + 1) * v->eltsize),
            (v->len - index) * v->eltsize);
    }
}
#endif
