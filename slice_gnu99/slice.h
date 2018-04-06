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

#define s_make_(T, len_, cap_) __extension__({ \
    slice(T) *sM__ = malloc(sizeof(*sM__)); \
    *sM__ = (slice(T)){malloc(sizeof(*sM__->base)), 0, len_}; \
    *sM__->base = (slice_base(T)){malloc(cap_ * sizeof(T)), cap_, 1}; \
    assert(cap_ > 0 && len_ <= cap_ && sM__->base->arr); \
    sM__; })
#define s_make(T, len, cap) __extension__({ \
    size_t lenM_ = len, capM_ = cap; \
    s_make_(T, lenM_, capM_); })

#define s_drop_(slice) __extension__({ \
    if (--slice->base->refs == 0) { \
        free(slice->base->arr); \
        free(slice->base); \
    } \
    free(slice); \
    NULL; })
#define s_drop(slice) __extension__({ \
    __auto_type sD_ = slice; \
    s_drop_(sD_); })

#define s_arr_(slice) (slice->base->arr + slice->offset)
#define s_arr(slice) __extension__({ \
    __auto_type sA_ = slice; \
    s_arr_(sA_); })

#define s_get_(slice, index) \
    (assert((size_t)index < slice->len), s_arr_(slice)[index])
#define s_get(slice, index) __extension__({ \
    __auto_type sG_ = slice; \
    __auto_type iG_ = index; \
    s_get_(sG_, iG_); })

#define s_put_(slice, index, elt) \
    (assert((size_t)index < slice->len), (void)(s_arr_(slice)[index] = elt))
#define s_put(slice, index, elt) __extension__({ \
    __auto_type sP_ = slice; \
    __auto_type iP_ = index; \
    __auto_type eP_ = elt; \
    s_put_(sP_, iP_, eP_); })

#define s_grow_(base, cap_) do { \
    base->arr = realloc(base->arr, (base->cap = cap_) * sizeof(*base->arr)); \
    assert(base); } while (0)

#define s_append_(slice, elt) do { \
    if (slice->len + slice->offset == slice->base->cap) { \
        s_grow_(slice->base, 2 * slice->base->cap); \
    } \
    s_arr_(slice)[slice->len++] = elt; } while (0)
#define s_append(slice, elt) do { \
    __extension__ __auto_type sAP_ = slice; \
    __extension__ __auto_type eAP_ = elt; \
    s_append_(sAP_, eAP_); } while (0)

#define s_concat_(slice1, slice2) do { \
    while (slice1->len + slice1->offset + slice2.len > slice1->base->cap) { \
        s_grow_(slice1->base, 2 * slice1->base->cap); \
    } \
    memmove(s_arr_(slice1) + slice1->len, \
            slice2.base->arr + slice2.offset, \
            slice2.len * sizeof(*slice1->base->arr)); \
    slice1->len += slice2.len; } while (0)
#define s_concat(slice1, slice2) do { \
    __extension__ __auto_type s1C_ = slice1; \
    typeof(*slice1) s2C_ = slice2; \
    s_concat_(s1C_, s2C_); } while (0)

#define s_slice_(slice, head, tail) __extension__({ \
    slice->base->refs++; \
    typeof(slice) sS__ = malloc(sizeof(*sS__)); \
    assert(head < tail && tail <= slice->len && sS__); \
    sS__->base = slice->base; \
    sS__->offset = slice->offset + head; \
    sS__->len = tail - head; \
    sS__; })
#define s_slice(slice, head, tail) __extension__({ \
    __auto_type sS_ = slice; \
    size_t hS_ = head, tS_ = tail; \
    s_slice_(sS_, hS_, tS_); })

#define s_slice_from_n_(arr_, T, head, tail, cap) __extension__({ \
    assert(head < tail && tail - head <= cap); \
    slice(T) *sSFN__ = s_make_(T, tail - head, cap); \
    memcpy(sSFN__->base->arr, arr_ + head, (tail - head) * sizeof(T)); \
    sSFN__; })
#define s_slice_from_n(arr, T, head, tail, cap) __extension__({ \
    size_t headSFN_ = head, tailSFN_ = tail, capSFN_ = cap; \
    s_slice_from_n_(arr, T, headSFN_, tailSFN_, capSFN_); })

#define s_slice_from_(arr, T, head, tail) \
    s_slice_from_n_(arr, T, head, tail, 2 * (tail - head))
#define s_slice_from(arr, T, head, tail) __extension__({ \
    size_t headSF_ = head, tailSF_ = tail; \
    s_slice_from_(arr, T, headSF_, tailSF_); })

#define s_slice_of_(slice, head, tail) \
    (assert(head < tail && tail <= slice->len), \
     (typeof(*slice)){slice->base, slice->offset + head, tail - head})
#define s_slice_of(slice, head, tail) __extension__({ \
    __auto_type sSO_ = slice; \
    size_t headSO_ = head, tailSO_ = tail; \
    s_slice_of_(sSO_, headSO_, tailSO_); })

#define s_find_(slice, elt) __extension__({ \
    ssize_t foundF__ = -1; \
    for (size_t i = 0; i < slice->len; i++) { \
        if (s_arr_(slice)[i] == elt) { \
            foundF__ = i; \
            break; \
        } \
    } \
    foundF__; })
#define s_find(slice, elt) __extension__({ \
    __auto_type sF_ = slice; \
    __auto_type eF_ = elt; \
    s_find_(sF_, eF_); })

#define s_remove_(slice, index) __extension__({ \
    bool s_removedR_ = (index) >= 0; \
    if (s_removedR_) { \
        slice->len--; \
        if (index == 0) { \
            slice->offset++; \
        } else { \
            size_t iR__ = index; \
            assert(iR__ <= slice->len); \
            if (iR__ != slice->len) { \
                memmove(s_arr_(slice) + iR__, \
                        s_arr_(slice) + iR__ + 1, \
                        (slice->len - iR__) * sizeof(*slice->base->arr)); \
            } \
        } \
    } \
    s_removedR_; })
#define s_remove(slice, index) __extension__({ \
    __auto_type sR_ = slice; \
    __auto_type iR_ = index; \
    s_remove_(sR_, iR_); })

#endif
