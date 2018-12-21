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
#define VECTOR_H_GNU

/* Each individual vector type must be defined before use. Pointer types must
 * be wrapped with `ptr` instead of using `*` as type names are created via
 * token pasting. E.g. `VECTOR_DEF_PTR(int);` defines the type of vectors of
 * pointers to integers and `vector(ptr(int)) v;` declares one such vector. */
#define vector(T) vector_##T##_
#ifndef MINIMAL_
#define MINIMAL_
#define ptr(T) T##ptr_
#endif

#define VECTOR_DEF(T) \
    typedef union vector(T) { \
        vector_hdr_ hdr; \
        struct { \
            T *arr; \
            size_t len, cap; \
        } vec; \
    } vector(T)
#define VECTOR_DEF_PTR(T) \
    typedef T *ptr(T); \
    VECTOR_DEF(ptr(T))

/* Exported "functions" */
#define vec_make(T, len, cap) ((vector(T) *)vector_make(sizeof(T), len, cap))
#define vec_drop(v) vector_drop(&v->hdr)
#define vec_shrink(v) vector_shrink(&v->hdr, sizeof(*v->vec.arr))
#define vec_trim(v) vector_trim(&v->hdr, sizeof(*v->vec.arr))

#define vec_len(v) (v->vec.len)
#define vec_arr(v) (v->vec.arr)
#define vec_index(v, index) (*vec_index_(v, index, __COUNTER__))

#define vec_push(v, elt) vec_push_(v, elt, __COUNTER__)
#define vec_peek(v, elt) vec_peek_(v, elt, __COUNTER__)
#define vec_pop(v, elt) vec_pop_(v, elt, __COUNTER__)

/* The `sizeof` is a type check; `v = v1` doesn't actually get evaluated. */
#define vec_concat(v, v1) \
    do { \
        (void)sizeof((v = v1)); \
        vector_concat(&v->hdr, &v1->hdr, sizeof(*v->vec.arr)); \
    } while (0)
#define vec_find(v, elt) vec_find_(v, elt, __COUNTER__)
#define vec_remove(v, index) vector_remove(&v->hdr, index, sizeof(*v->vec.arr))

/* These declarations must be present in exactly one compilation unit. */
#define VECTOR_EXTERN_DECL \
    extern inline void vector_assert_( \
        const char *, unsigned, const char *) __attribute__((noreturn)); \
    extern inline void *vector_make(size_t, size_t, size_t); \
    extern inline void *vector_drop(vector_hdr_ *); \
    extern inline void vector_shrink(vector_hdr_ *, size_t); \
    extern inline void vector_trim(vector_hdr_ *, size_t); \
    extern inline void vector_grow_(vector_hdr_ *, size_t); \
    extern inline void vector_concat(vector_hdr_ *, vector_hdr_ *, size_t); \
    extern inline void vector_remove(vector_hdr_ *, size_t, size_t)

/* ---------------------------- Implementation ---------------------------- */
typedef struct vector_hdr_ {
    char *arr;
    size_t len, cap;
} vector_hdr_;

/* Almost hygenic... */
#define vec_sym_(sym, id) VEC_##sym##id##_

/* GCC and Clang both seem to do a good job of of eliminating any unnecessary
 * variables at O1 and above. */
#define vec_index_(v, index, id) \
    __extension__ ({ \
        __auto_type vec_sym_(v, id) = &v->vec; \
        __auto_type vec_sym_(index, id) = index; \
        vec_assert_( \
            vec_sym_(index, id) < (typeof(index))vec_sym_(v, id)->len); \
        vec_sym_(v, id)->arr + vec_sym_(index, id); \
    })

#define vec_push_(v, elt, id) \
    do { \
        __extension__ __auto_type vec_sym_(v, id) = v; \
        if (vec_sym_(v, id)->vec.len == vec_sym_(v, id)->vec.cap) { \
            vector_grow_(&vec_sym_(v, id)->hdr, sizeof(*v->vec.arr)); \
        } \
        vec_sym_(v, id)->vec.arr[vec_sym_(v, id)->vec.len++] = elt; \
    } while (0)

#define vec_peek_(v, elt, id) \
    __extension__ ({ \
        __label__ ret; \
        bool vec_sym_(rc, id) = true; \
        __auto_type vec_sym_(v, id) = &v->vec; \
        if (vec_sym_(v, id)->len == 0) { \
            vec_sym_(rc, id) = false; \
            goto ret; \
        } \
        elt = vec_sym_(v, id)->arr[vec_sym_(v, id)->len - 1]; \
ret: \
        vec_sym_(rc, id); \
    })

#define vec_pop_(v, elt, id) \
    __extension__ ({ \
        __label__ ret; \
        __auto_type vec_sym_(v, id) = &v->vec; \
        bool vec_sym_(rc, id) = true; \
        if (vec_sym_(v, id)->len == 0) { \
            vec_sym_(rc, id) = false; \
            goto ret; \
        } \
        elt = vec_sym_(v, id)->arr[--vec_sym_(v, id)->len]; \
ret: \
        vec_sym_(rc, id); \
    })

#define vec_find_(v, elt, id) \
    __extension__ ({ \
        __label__ ret; \
        __auto_type vec_sym_(v, id) = &v->vec; \
        __auto_type vec_sym_(elt, id) = elt; \
        size_t vec_sym_(index, id) = SIZE_MAX; \
        for (size_t i = 0; i < vec_sym_(v, id)->len; i++) { \
            if (vec_sym_(v, id)->arr[i] == vec_sym_(elt, id)) { \
                vec_sym_(index, id) = i; \
                goto ret; \
            } \
        } \
ret: \
        vec_sym_(index, id); \
    })

/* `vec_assert_` never becomes a noop, even when `NDEBUG` is set. */
#define vec_assert_(pred) \
    (__builtin_expect(!(pred), 0) ? \
        vector_assert_(__FILE__, __LINE__, #pred) : (void)0)

__attribute__((noreturn)) inline void
vector_assert_(const char *file, unsigned line, const char *pred) {
    fprintf(stderr, "Failed assertion: %s, %u, %s\n", file, line, pred);
    abort();
}

inline void *
vector_make(size_t eltsize, size_t len, size_t cap) {
    vec_assert_(
        eltsize <= SIZE_MAX / 8 && // Ugh lol; `see vector_grow_`
        cap <= SIZE_MAX / eltsize &&
        len <= cap);
    vector_hdr_ *v = malloc(sizeof(*v));
    vec_assert_(v);
    *v = (vector_hdr_){malloc(cap * eltsize), len, cap};
    vec_assert_(v->arr || cap == 0);
    return v;
}

inline void *
vector_drop(vector_hdr_ *v) {
    free(v->arr);
    free(v);
    return NULL;
}

inline void
vector_shrink(vector_hdr_ *v, size_t eltsize) {
    if (v->len * 4 > v->cap) {
        return;
    }
    vec_assert_(
        (v->arr = realloc(v->arr, (v->cap = 2 * v->len) * eltsize)) ||
        v->len == 0);
}

inline void
vector_trim(vector_hdr_ *v, size_t eltsize) {
    if (v->len == v->cap) {
        return;
    }
    vec_assert_(
        (v->arr = realloc(v->arr, (v->cap = v->len) * eltsize)) ||
        v->len == 0);
}

inline void
vector_grow_(vector_hdr_ *v, size_t eltsize) {
    if (v->cap == 0) {
        v->cap = 8;
    } else {
        vec_assert_(v->cap < SIZE_MAX - v->cap &&
                (v->cap *= 2) <= SIZE_MAX / eltsize);
    }
    vec_assert_((v->arr = realloc(v->arr, v->cap * eltsize)));
}

inline void
vector_concat(vector_hdr_ *v, vector_hdr_ *v1, size_t eltsize) {
    if (v->len + v1->len > v->cap) {
        size_t g = v->cap > v1->cap ? v->cap : v1->cap;
        vec_assert_(v->cap < SIZE_MAX - g &&
                (v->cap += g) <= SIZE_MAX / eltsize);
        vec_assert_((v->arr = realloc(v->arr, v->cap * eltsize)));
    }
    memmove(v->arr + (v->len * eltsize), v1->arr, v1->len * eltsize);
    v->len += v1->len;
}

inline void
vector_remove(vector_hdr_ *v, size_t index, size_t eltsize) {
    if (index == SIZE_MAX) {
        return;
    }
    v->len--;
    vec_assert_(index <= v->len);
    if (index != v->len) {
        memmove(
            v->arr + (index * eltsize),
            v->arr + ((index + 1) * eltsize),
            (v->len - index) * eltsize);
    }
}
#endif
