/* Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file. */
#ifndef SLICE_H
#define SLICE_H
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MACRO_CONCAT_(x, y) x##y
#define MC_(x, y) MACRO_CONCAT_(x, y)

#define slice(T) MC_(slice_, T)
#define slice_base(T) struct slice_base_##T
#define PTR_OF(T) MC_(T, ptr_)
#define SLICE_DEF(T) \
    typedef struct slice(T) { \
        slice_base(T) { \
            T *arr; \
            size_t cap, refs; \
        } *base; \
        size_t offset, len; \
    } slice(T)
#define SLICE_DEF_PTR(T) \
    typedef T *PTR_OF(T); \
    SLICE_DEF(PTR_OF(T))

#define MAKE_(T, len_, cap_) __extension__({ \
    slice(T) *sM__ = malloc(sizeof(*sM__)); \
    *sM__ = (slice(T)){malloc(sizeof(*sM__->base)), 0, len_}; \
    *sM__->base = (slice_base(T)){malloc(cap_ * sizeof(T)), cap_, 1}; \
    assert(cap_ > 0 && len_ <= cap_ && sM__->base->arr); \
    sM__; })
#define MAKE(T, len, cap) __extension__({ \
    size_t lenM_ = len, capM_ = cap; \
    MAKE_(T, lenM_, capM_); })

#define DROP_(slice) __extension__({ \
    if (--slice->base->refs == 0) { \
        free(slice->base->arr); \
        free(slice->base); \
    } \
    free(slice); \
    NULL; })
#define DROP(slice) __extension__({ \
    __auto_type sD_ = slice; \
    DROP_(sD_); })

#define ARR_(slice) (slice->base->arr + slice->offset)
#define ARR(slice) __extension__({ \
    __auto_type sA_ = slice; \
    ARR_(sA_); })

#define GET_(slice, index) \
    (assert((size_t)index < slice->len), ARR_(slice)[index])
#define GET(slice, index) __extension__({ \
    __auto_type sG_ = slice; \
    __auto_type iG_ = index; \
    GET_(sG_, iG_); })

#define PUT_(slice, index, elt) \
    (assert((size_t)index < slice->len), (void)(ARR_(slice)[index] = elt))
#define PUT(slice, index, elt) __extension__({ \
    __auto_type sP_ = slice; \
    __auto_type iP_ = index; \
    __auto_type eP_ = elt; \
    PUT_(sP_, iP_, eP_); })

#define GROW_(base, cap_) do { \
    base->arr = realloc(base->arr, (base->cap = cap_) * sizeof(*base->arr)); \
    assert(base); } while (0)

#define APPEND_(slice, elt) do { \
    if (slice->len + slice->offset == slice->base->cap) { \
        GROW_(slice->base, 2 * slice->base->cap); \
    } \
    ARR_(slice)[slice->len++] = elt; } while (0)
#define APPEND(slice, elt) do { \
    __extension__ __auto_type sAP_ = slice; \
    __extension__ __auto_type eAP_ = elt; \
    APPEND_(sAP_, eAP_); } while (0)

#define CONCAT_(slice1, slice2) do { \
    while (slice1->len + slice1->offset + slice2.len > slice1->base->cap) { \
        GROW_(slice1->base, 2 * slice1->base->cap); \
    } \
    memmove(ARR_(slice1) + slice1->len, \
            slice2.base->arr + slice2.offset, \
            slice2.len * sizeof(*slice1->base->arr)); \
    slice1->len += slice2.len; } while (0)
#define CONCAT(slice1, slice2) do { \
    __extension__ __auto_type s1C_ = slice1; \
    typeof(*slice1) s2C_ = slice2; \
    CONCAT_(s1C_, s2C_); } while (0)

#define SLICE_(slice, head, tail) __extension__({ \
    slice->base->refs++; \
    typeof(slice) sS__ = malloc(sizeof(*sS__)); \
    assert(head < tail && tail <= slice->len && sS__); \
    sS__->base = slice->base; \
    sS__->offset = slice->offset + head; \
    sS__->len = tail - head; \
    sS__; })
#define SLICE(slice, head, tail) __extension__({ \
    __auto_type sS_ = slice; \
    size_t hS_ = head, tS_ = tail; \
    SLICE_(sS_, hS_, tS_); })

#define SLICE_FROM_N_(arr_, T, head, tail, cap) __extension__({ \
    assert(head < tail && tail - head <= cap); \
    slice(T) *sSFN__ = MAKE_(T, tail - head, cap); \
    memcpy(sSFN__->base->arr, arr_ + head, (tail - head) * sizeof(T)); \
    sSFN__; })
#define SLICE_FROM_N(arr, T, head, tail, cap) __extension__({ \
    size_t headSFN_ = head, tailSFN_ = tail, capSFN_ = cap; \
    SLICE_FROM_N_(arr, T, headSFN_, tailSFN_, capSFN_); })

#define SLICE_FROM_(arr, T, head, tail) \
    SLICE_FROM_N_(arr, T, head, tail, 2 * (tail - head))
#define SLICE_FROM(arr, T, head, tail) __extension__({ \
    size_t headSF_ = head, tailSF_ = tail; \
    SLICE_FROM_(arr, T, headSF_, tailSF_); })

#define SLICE_OF_(slice, head, tail) \
    (assert(head < tail && tail <= slice->len), \
     (typeof(*slice)){slice->base, slice->offset + head, tail - head})
#define SLICE_OF(slice, head, tail) __extension__({ \
    __auto_type sSO_ = slice; \
    size_t headSO_ = head, tailSO_ = tail; \
    SLICE_OF_(sSO_, headSO_, tailSO_); })

#define FIND_(slice, elt) __extension__({ \
    ssize_t foundF__ = -1; \
    for (size_t i = 0; i < slice->len; i++) { \
        if (ARR_(slice)[i] == elt) { \
            foundF__ = i; \
            break; \
        } \
    } \
    foundF__; })
#define FIND(slice, elt) __extension__({ \
    __auto_type sF_ = slice; \
    __auto_type eF_ = elt; \
    FIND_(sF_, eF_); })

#define REMOVE_(slice, index) __extension__({ \
    bool removedR_ = (index) >= 0; \
    if (removedR_) { \
        slice->len--; \
        if (index == 0) { \
            slice->offset++; \
        } else { \
            size_t iR__ = index; \
            assert(iR__ <= slice->len); \
            if (iR__ != slice->len) { \
                memmove(ARR_(slice) + iR__, \
                        ARR_(slice) + iR__ + 1, \
                        (slice->len - iR__) * sizeof(*slice->base->arr)); \
            } \
        } \
    } \
    removedR_; })
#define REMOVE(slice, index) __extension__({ \
    __auto_type sR_ = slice; \
    __auto_type iR_ = index; \
    REMOVE_(sR_, iR_); })

#endif
