/* vector.h v0.0.0
 * Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef VECTOR_H
#define VECTOR_H
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------- Interface ------------------------------- */
#define vector(T) vector_##T##_

#define vec_make(T, len, cap) vector_make(sizeof(T), len, cap)
#define vec_drop(v) vector_drop(&v->hdr)

#define vec_arr(v) (v->vec.arr)
#define vec_len(v) (v->vec.len)
#define vec_index(v, index) (*vec_index_(v, index, __COUNTER__))

#define vec_push(v, elt) vec_push_(v, elt, __COUNTER__)
#define vec_pop(v, elt) vec_pop_(v, elt, __COUNTER__)

#define vec_concat(v, v1) vector_concat(&v->hdr, &v1->hdr)
#define vec_find(v, elt) vec_find_(v, elt, __COUNTER__)
#define vec_remove(v, index) vector_remove(&v->hdr, index)

#ifndef MINIMAL_
#define ptr(T) T##ptr_
#endif

#define VECTOR_DEF(T) \
    typedef union vector(T) { \
        vector_hdr_ hdr; \
        struct { \
            T *arr; \
            size_t eltsize, len, cap; \
        } vec; \
    } vector(T)
#define VECTOR_DEF_PTR(T) \
    typedef T * ptr(T); \
    VECTOR_DEF(ptr(T))

/* These declarations must be present in exactly one compilation unit. */
#define VECTOR_EXTERN_DECL \
    extern inline void vector_assert_( \
        const char *, unsigned, const char *) __attribute__((noreturn)); \
    extern inline void *vector_make(size_t, size_t, size_t); \
    extern inline void *vector_drop(vector_hdr_ *); \
    extern inline void vector_concat(vector_hdr_ *, vector_hdr_ *); \
    extern inline void vector_remove(vector_hdr_ *, ssize_t)

/* ---------------------------- Implementation ---------------------------- */
typedef struct vector_hdr_ {
    char *arr;
    size_t eltsize, len, cap;
} vector_hdr_;

/* Almost hygenic... */
#define vec_sym_(sym, id) VEC_##sym##id##_

/* GCC and Clang both seem to do a good job of of eliminating any unnecessary
 * variables at O1 and above. */
#define vec_index_(v, index, id) \
    __extension__ ({ \
        __auto_type vec_sym_(v, id) = &v->vec; \
        vec_assert_(index < vec_sym_(v, id)->len); \
        vec_sym_(v, id)->arr + index; \
    })

#define vec_push_(v, elt, id) \
    do { \
        __extension__ __auto_type vec_sym_(v, id) = &v->vec; \
        if (vec_sym_(v, id)->len == vec_sym_(v, id)->cap) { \
            vec_assert_((vec_sym_(v, id)->arr = realloc( \
                vec_sym_(v, id)->arr, \
                (vec_sym_(v, id)->cap *= 2) * \
                        sizeof(*vec_sym_(v, id)->arr)))); \
        } \
        vec_sym_(v, id)->arr[vec_sym_(v, id)->len++] = elt; \
    } while (0)

#define vec_pop_(v, elt, id) \
    __extension__ ({ \
        __label__ ret; \
        bool vec_sym_(rc, id) = true; \
        __auto_type vec_sym_(v, id) = &v->vec; \
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
        bool vec_sym_(rc, id) = false; \
        __auto_type vec_sym_(v, id) = &v->vec; \
        for (size_t i = 0; i < vec_sym_(v, id)->len; i++) { \
            if (vec_sym_(v, id)->arr[i] == elt) { \
                vec_sym_(rc, id) = i; \
                goto ret; \
            } \
        } \
ret: \
        vec_sym_(rc, id); \
    })

/* vec_assert_ never becomes a noop, even when NDEBUG is set. */
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
    vector_hdr_ *v = malloc(sizeof(*v));
    vec_assert_(cap > 0 && len <= cap && v);
    *v = (vector_hdr_){malloc(cap * eltsize), eltsize, len, cap};
    vec_assert_(v->arr);
    return v;
}

inline void *
vector_drop(vector_hdr_ *v) {
    free(v->arr);
    free(v);
    return NULL;
}

inline void
vector_concat(vector_hdr_ *v, vector_hdr_ *v1) {
    vec_assert_(v->eltsize == v1->eltsize);
    if (v->len + v1->len > v->cap) {
        vec_assert_((v->arr = realloc(
            v->arr,
            v->eltsize * (v->cap += v->cap > v1->cap ? v->cap : v1->cap))));
    }
    memmove(v->arr + (v->len * v->eltsize), v1->arr, v1->len * v1->eltsize);
    v->len += v1->len;
}

inline void
vector_remove(vector_hdr_ *v, ssize_t index) {
    size_t i = index;
    if (index < 0) {
        return;
    }
    v->len--;
    vec_assert_(i <= v->len);
    if (i != v->len) {
        memmove(
            v->arr + (i * v->eltsize),
            v->arr + ((i + 1) * v->eltsize),
            (v->len - i) * v->eltsize);
    }
}
#endif
