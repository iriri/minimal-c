/* vector.h v0.0.0
 * Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef VECTOR_H
#define VECTOR_H
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------- Interface ------------------------------- */
typedef struct vector vector;

#define vec_make(type, len, cap) vector_make(sizeof(type), len, cap)
#define vec_drop(v) vector_drop(v)

#define vec_arr(v, type) vec_arr_(v, type, __COUNTER__)
#define vec_put(v, index, elt) vec_put_(v, index, elt, __COUNTER__)
#define vec_get(v, index, elt) vec_get_(v, index, elt, __COUNTER__)
#define vec_append(v, elt) vec_append_(v, elt, __COUNTER__)

#define vec_concat(v1, v2) vector_concat(v1, v2)
#define vec_find(v, elt) vector_find(v, &elt)
#define vec_remove(v, index) vector_remove(v, index)

/* These declarations must be present in exactly one compilation unit. */
#define VECTOR_EXTERN_DECL \
    extern inline void vector_assert_( \
        const char *, unsigned, const char *) __attribute__((noreturn)); \
    extern inline vector *vector_make(size_t, size_t, size_t); \
    extern inline vector *vector_drop(vector *); \
    extern inline void *vector_index(vector *, size_t); \
    extern inline void *vector_append(vector *); \
    extern inline void vector_concat(vector *, vector *); \
    extern inline ssize_t vector_find(vector *, void *); \
    extern inline void vector_remove(vector *, ssize_t)

/* ---------------------------- Implementation ---------------------------- */
struct vector {
    char *arr;
    size_t eltsize, len, cap;
};

/* Almost hygenic... */
#define vec_mc_(a, b) a##b

/* GCC and Clang both do a good job of of eliminating any unnecessary variables
 * at O1 and above. With NDEBUG, pretty much everything appears to be
 * eliminated, as desired (needs more verification). (sizeof(typeof(elt)) is
 * unfortunately required due to integer promotion on character literals. */
#define vec_arr_(v, type, uniq) \
    __extension__ ({ \
        vector *vec_mc_(VEC_v, uniq) = v; \
        assert(sizeof(type) == vec_mc_(VEC_v, uniq)->eltsize); \
        (type *)vec_mc_(VEC_v, uniq)->arr; \
    })
#define vec_put_(v, index, elt, uniq) \
    do { \
        vector *vec_mc_(VEC_v, uniq) = v; \
        assert(sizeof(__typeof__(elt)) == vec_mc_(VEC_v, uniq)->eltsize); \
        *(__typeof__(elt) *)vector_index(vec_mc_(VEC_v, uniq), index) = elt; \
    } while (0)
#define vec_get_(v, index, elt, uniq) \
    do { \
        vector *vec_mc_(VEC_v, uniq) = v; \
        assert(sizeof(__typeof__(elt)) == vec_mc_(VEC_v, uniq)->eltsize); \
        elt = *(__typeof__(elt) *)vector_index(vec_mc_(VEC_v, uniq), index); \
    } while (0)
#define vec_append_(v, elt, uniq) \
    do { \
        vector *vec_mc_(VEC_v, uniq) = v; \
        assert(sizeof(__typeof__(elt)) == vec_mc_(VEC_v, uniq)->eltsize); \
        *(__typeof__(elt) *)vector_append(vec_mc_(VEC_v, uniq)) = elt; \
    } while (0)

/* vec_assert_ never becomes a noop, even when NDEBUG is set. */
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
    vector *v = malloc(sizeof(*v));
    vec_assert_(cap > 0 && len <= cap && v);
    *v = (vector){malloc(cap * eltsize), eltsize, len, cap};
    vec_assert_(v->arr);
    return v;
}

inline vector *
vector_drop(vector *v) {
    free(v->arr);
    free(v);
    return NULL;
}

inline void *
vector_index(vector *v, size_t index) {
    vec_assert_(index < v->len);
    return v->arr + (index * v->eltsize);
}

inline void *
vector_append(vector *v) {
    if (v->len == v->cap) {
        vec_assert_((v->arr = realloc(v->arr, (v->cap *= 2) * v->eltsize)));
    }
    return v->arr + (v->len++ * v->eltsize);
}

inline void
vector_concat(vector *v, vector *v1) {
    assert(v->eltsize == v1->eltsize);
    if (v->len + v1->len > v->cap) {
        vec_assert_((v->arr = realloc(
            v->arr,
            v->eltsize * (v->cap += v->cap > v1->cap ? v->cap : v1->cap))));
    }
    memmove(v->arr + (v->len * v->eltsize), v1->arr, v1->len * v1->eltsize);
    v->len += v1->len;
}

inline ssize_t
vector_find(vector *v, void *elt) {
    for (size_t i = 0; i < v->len; i++) {
        if (memcmp(v->arr + (i * v->eltsize), elt, v->eltsize) == 0) {
            return i;
        }
    }
    return -1;
}

inline void
vector_remove(vector *v, ssize_t index) {
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
