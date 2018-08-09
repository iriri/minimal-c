/* minmax.h v0.0.0
 * Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by a BSD-style license which can be found in the LICENSE file.
 *
 * This library is an implementation of the data structure described in
 * "Min-Max Heaps and Generalized Priority Queues" by M.D. Atkinson, J.-R.
 * Sack, N. Santoro, and T. Strothott. */
#ifndef MINMAX_H
#define MINMAX_H
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------- Interface ------------------------------- */
#define MINMAX_H_VERSION 0l // 0.0.0

typedef struct minmax minmax;
typedef int (*minmax_cmpfn)(void *restrict, void *restrict);

/* Exported "functions" */
#define mm_make(T, cap, cmpfn) minmax_make(sizeof(T), cap, cmpfn)
#define mm_drop(m) minmax_drop(m)
#define mm_fromarr(T, len, cap, cmpfn, arr) \
    minmax_fromarr(sizeof(T), len, cap, cmpfn, arr)
#define mm_shrink(m) minmax_shrink(m)

#define mm_len(m) (m->len)

/* `__COUNTER__`, unfortunately, is an extension. `__LINE__` is mostly good
 * enough except when nesting similar macros in one another. */
#define mm_insert(m, T, elt) mm_insert_(m, T, elt, __LINE__)

/* TODO Is there a way to do this "type check" at compile-time? `static_assert`
 * is a statement, not an expression, so it can't be used with the comma
 * operator. Is this weak of a "type check" even worth doing? */
#define mm_peekmin(m, T, elt) \
    ( \
        mm_assert_(sizeof(T) == sizeof(elt)), \
        minmax_peekmin(m, sizeof(T), &elt, false) \
    )
#define mm_pollmin(m, T, elt) \
    ( \
        mm_assert_(sizeof(T) == sizeof(elt)), \
        minmax_peekmin(m, sizeof(T), &elt, true) \
    )
#define mm_peekmax(m, T, elt) \
    ( \
        mm_assert_(sizeof(T) == sizeof(elt)), \
        minmax_peekmax(m, sizeof(T), &elt, false) \
    )
#define mm_pollmax(m, T, elt) \
    ( \
        mm_assert_(sizeof(T) == sizeof(elt)), \
        minmax_peekmax(m, sizeof(T), &elt, true) \
    )

/* These declarations must be present in exactly one compilation unit. */
#define MINMAX_EXTERN_DECL \
    extern inline void minmax_assert_( \
        const char *, unsigned, const char *) __attribute__((noreturn)); \
    extern inline minmax *minmax_make( \
        size_t, size_t, minmax_cmpfn); \
    extern inline minmax *minmax_drop(minmax *); \
    extern inline void minmax_shrink(minmax *); \
    extern inline void *minmax_push_(minmax *, size_t); \
    extern inline size_t minmax_parent_(size_t); \
    extern inline bool minmax_level_type_max_(size_t); \
    extern inline void minmax_swap_elts_( \
        void *restrict, void *restrict, size_t); \
    extern inline void minmax_bubble_up_min_(minmax *, size_t, size_t); \
    extern inline void minmax_bubble_up_max_(minmax *, size_t, size_t); \
    extern inline void minmax_bubble_up_(minmax *, size_t, size_t); \
    extern inline size_t minmax_child_(size_t); \
    extern inline void minmax_trickle_down_min_(minmax *, size_t, size_t); \
    extern inline void minmax_trickle_down_max_(minmax *, size_t, size_t); \
    extern inline void minmax_trickle_down_(minmax *, size_t, size_t); \
    extern inline minmax *minmax_fromarr( \
        size_t, size_t, size_t, minmax_cmpfn, void *); \
    extern inline bool minmax_peekmin(minmax *, size_t, void *, bool); \
    extern inline bool minmax_peekmax(minmax *, size_t, void *, bool)

/* ---------------------------- Implementation ---------------------------- */
struct minmax {
    char *heap;
    size_t eltsize, len, cap;
    /* Is this a good idea? Could do offset + keylen instead. */
    minmax_cmpfn cmpfn;
};

/* Almost hygenic... */
#define mm_sym_(sym, id) MM_##sym##id##_

#define mm_insert_(m, T, elt, id) \
    do { \
        mm_assert_(sizeof(T) == sizeof(elt)); \
        minmax *mm_sym_(m, id) = m; \
        *(T *)minmax_push_(mm_sym_(m, id), sizeof(T)) = elt; \
        minmax_bubble_up_(mm_sym_(m, id), mm_sym_(m, id)->len++, sizeof(T)); \
    } while (0)

/* `mm_assert_` never becomes a noop, even when `NDEBUG` is set. */
#define mm_assert_(pred) \
    (__builtin_expect(!(pred), 0) ? \
        minmax_assert_(__FILE__, __LINE__, #pred) : (void)0)

#define mm_elt_(index) m->heap + ((index) * eltsize)

__attribute__((noreturn)) inline void
minmax_assert_(const char *file, unsigned line, const char *pred) {
    fprintf(stderr, "Failed assertion: %s, %u, %s\n", file, line, pred);
    abort();
}

/* Allocates and initializes a new min-max heap with the specified initial
 * capacity. `cmpfn` should return a negative integer, zero, or a positive
 * integer when the two elements are less than, equal to, or greater than each
 * other, respectively. */
inline minmax *
minmax_make(size_t eltsize, size_t cap, minmax_cmpfn cmpfn) {
    mm_assert_(0 < cap && cap < SIZE_MAX / eltsize);
    minmax *m = malloc(sizeof(*m));
    mm_assert_(m);
    *m = (minmax) {malloc(cap * eltsize), eltsize, 0, cap, cmpfn};
    mm_assert_(m->heap);
    return m;
}

/* Deallocates all resources associated with the heap and returns `NULL`. */
inline minmax *
minmax_drop(minmax *m) {
    free(m->heap);
    free(m);
    return NULL;
}

/* Shrinks the allocation of the heap to twice its length if it is currently
 * more than 75% empty. Does nothing otherwise. This is the only way to shrink
 * the allocation of the heap--it never shrinks itself automatically. */
inline void
minmax_shrink(minmax *m) {
    if (m->len * 4 > m->cap) {
        return;
    }
    mm_assert_((m->heap = realloc(
        m->heap, (m->cap = 2 * m->len) * m->eltsize)));
}

inline void *
minmax_push_(minmax *m, size_t eltsize) {
    mm_assert_(eltsize == m->eltsize);
    if (m->len == m->cap) {
        mm_assert_(m->cap < SIZE_MAX - m->cap &&
                (m->cap *= 2) < SIZE_MAX / eltsize);
        mm_assert_((m->heap = realloc(m->heap, m->cap * eltsize)));
    }
    return mm_elt_(m->len);
}

inline size_t
minmax_parent_(size_t index) {
    if (index == 0 || index == SIZE_MAX) {
        return SIZE_MAX;
    }
    return (index - 1) / 2;
}

inline bool
minmax_level_type_max_(size_t index) {
    return (unsigned)(log2(index + 1)) % 2; // Is this okay lol
}

inline void
minmax_swap_elts_(void *restrict elt, void *restrict elt1, size_t eltsize) {
    char tmp[eltsize]; // Time to blow the stack lmao
    memcpy(tmp, elt, eltsize);
    memcpy(elt, elt1, eltsize);
    memcpy(elt1, tmp, eltsize);
}

inline void
minmax_bubble_up_min_(minmax *m, size_t index, size_t eltsize) {
    size_t pindex;
    char *elt, *parent;
    while ((pindex = minmax_parent_(minmax_parent_(index))) != SIZE_MAX &&
            m->cmpfn((elt = mm_elt_(index)), (parent = mm_elt_(pindex))) < 0) {
        minmax_swap_elts_(elt, parent, eltsize);
        index = pindex;
    }
}

inline void
minmax_bubble_up_max_(minmax *m, size_t index, size_t eltsize) {
    size_t pindex;
    char *elt, *parent;
    while ((pindex = minmax_parent_(minmax_parent_(index))) != SIZE_MAX &&
            m->cmpfn((elt = mm_elt_(index)), (parent = mm_elt_(pindex))) > 0) {
        minmax_swap_elts_(elt, parent, eltsize);
        index = pindex;
    }
}

/* Duplicating the min and max versions yields slightly better performance. */
inline void
minmax_bubble_up_(minmax *m, size_t index, size_t eltsize) {
    size_t pindex;
    if ((pindex = minmax_parent_(index)) == SIZE_MAX) {
        return;
    }

    char *elt = mm_elt_(index);
    char *parent = mm_elt_(pindex);
    if (minmax_level_type_max_(index)) {
        if (m->cmpfn(elt, parent) < 0) {
            minmax_swap_elts_(elt, parent, eltsize);
            minmax_bubble_up_min_(m, pindex, eltsize);
            return;
        }
        minmax_bubble_up_max_(m, index, eltsize);
        return;
    }

    if (m->cmpfn(elt, parent) > 0) {
        minmax_swap_elts_(elt, parent, eltsize);
        minmax_bubble_up_max_(m, pindex, eltsize);
        return;
    }
    minmax_bubble_up_min_(m, index, eltsize);
}

inline size_t
minmax_child_(size_t index) {
    if (index == SIZE_MAX || index > ((SIZE_MAX - 2)/ 2)) {
        return SIZE_MAX;
    }
    return (2 * index) + 1;
}

inline void
minmax_trickle_down_min_(minmax *m, size_t index, size_t eltsize) {
    for ( ; ; ) {
        size_t cindex = minmax_child_(index);
        if (cindex >= m->len) {
            return;
        }

        size_t gindex = minmax_child_(cindex);
        if (cindex + 1 < m->len) {
            if (m->cmpfn(mm_elt_(cindex), mm_elt_((cindex + 1))) > 0) {
                cindex++;
            }
            if (gindex < m->len) {
                for (unsigned i = 0; gindex + i < m->len && i < 4; i++) {
                    if (m->cmpfn(mm_elt_(cindex), mm_elt_((gindex + i))) > 0) {
                        cindex = gindex + i;
                    }
                }
            }
        }

        char *elt = mm_elt_(index);
        char *child = mm_elt_(cindex);
        if (cindex < gindex) {
            if (m->cmpfn(child, elt) < 0) {
                minmax_swap_elts_(child, elt, eltsize);
            }
            return;
        }
        if (m->cmpfn(child, elt) >= 0) {
            return;
        }
        minmax_swap_elts_(child, elt, eltsize);
        char *parent = mm_elt_(minmax_parent_(cindex));
        if (m->cmpfn(child, parent) > 0) {
            minmax_swap_elts_(child, parent, eltsize);
        }
        index = cindex;
    }
}

inline void
minmax_trickle_down_max_(minmax *m, size_t index, size_t eltsize) {
    for ( ; ; ) {
        size_t cindex = minmax_child_(index);
        if (cindex >= m->len) {
            return;
        }

        size_t gindex = minmax_child_(cindex);
        if (cindex + 1 < m->len) {
            if (m->cmpfn(mm_elt_(cindex), mm_elt_((cindex + 1))) < 0) {
                cindex++;
            }
            if (gindex < m->len) {
                for (unsigned i = 0; gindex + i < m->len && i < 4; i++) {
                    if (m->cmpfn(mm_elt_(cindex), mm_elt_((gindex + i))) < 0) {
                        cindex = gindex + i;
                    }
                }
            }
        }

        char *elt = mm_elt_(index);
        char *child = mm_elt_(cindex);
        if (cindex < gindex) {
            if (m->cmpfn(child, elt) > 0) {
                minmax_swap_elts_(child, elt, eltsize);
            }
            return;
        }
        if (m->cmpfn(child, elt) <= 0) {
            return;
        }
        minmax_swap_elts_(child, elt, eltsize);
        char *parent = mm_elt_(minmax_parent_(cindex));
        if (m->cmpfn(child, parent) < 0) {
            minmax_swap_elts_(child, parent, eltsize);
        }
        index = cindex;
    }
}

/* Duplicating the min and max versions yields slightly better performance. */
inline void
minmax_trickle_down_(minmax *m, size_t index, size_t eltsize) {
    if (minmax_level_type_max_(index)) {
        minmax_trickle_down_max_(m, index, eltsize);
        return;
    }
    minmax_trickle_down_min_(m, index, eltsize);
}

/* Heapifies and takes ownership of an existing array. `arr` *must* be
 * dynamically allocated. */
inline minmax *
minmax_fromarr(
    size_t eltsize, size_t len, size_t cap, minmax_cmpfn cmpfn, void *arr
) {
    minmax *m = malloc(sizeof(*m));
    mm_assert_(cap > 0 && m);
    *m = (minmax) {arr, eltsize, len, cap, cmpfn};
    for (size_t i = len / 2; i != SIZE_MAX; i--) {
        minmax_trickle_down_(m, i, eltsize);
    }
    return m;
}

/* Peeks at the minimum element in the heap or removes it instead if `poll` is
 * `true`. */
inline bool
minmax_peekmin(minmax *m, size_t eltsize, void *elt, bool poll) {
    mm_assert_(eltsize == m->eltsize);
    if (m->len == 0) {
        return false;
    }
    memcpy(elt, m->heap, eltsize);
    if (poll && --m->len > 0) {
        memcpy(m->heap, mm_elt_(m->len), eltsize);
        minmax_trickle_down_(m, 0, eltsize);
    }
    return true;
}

/* Peeks at the maximum element in the heap or removes it instead if `poll` is
 * `true`. */
inline bool
minmax_peekmax(minmax *m, size_t eltsize, void *elt, bool poll) {
    mm_assert_(eltsize == m->eltsize);
    if (m->len == 0) {
        return false;
    }
    if (m->len < 3) {
        memcpy(elt, mm_elt_(poll ? --m->len : m->len - 1), eltsize);
        return true;
    }

    size_t index = m->cmpfn(mm_elt_(1), mm_elt_(2)) > 0 ?
            1 : 2;
    memcpy(elt, mm_elt_(index), eltsize);
    if (poll && --m->len > 0) {
        memcpy(mm_elt_(index), mm_elt_(m->len), eltsize);
        minmax_trickle_down_(m, index, eltsize);
    }
    return true;
}
#endif
