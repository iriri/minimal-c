## minmax.h
This library provides an implementation of a min-max-heap-based double-ended
priority queue.

Support for C99 is required.

### Types
```
typedef struct minmax minmax;
typedef int (*minmax_cmpfn)(void *restrict, void *restrict);
```

### Functions
#### mm_make / mm_drop
```
minmax *mm_make(type T, size_t cap, minmax_cmpfn cmpfn)

minmax *mm_drop(minmax *m)
```
`mm_make` allocates and initializes a new min-max heap with the specified
initial capacity and comparison function. `cmpfn` should return a negative
integer, zero, or a positive integer when the two elements are less than, equal
to, or greater than each other, respectively.

`mm_drop` deallocates all resources associated with the heap and returns
`NULL`.

#### mm_fromarr
```
minmax *mm_fromarr(type T, size_t len, size_t cap, minmax_cmpfn cmpfn, T *arr)
```
Heapifies and takes ownership of an existing array. `arr` *must* be dynamically
allocated.

#### mm_shrink
```
void mm_shrink(minmax *m)
```
Shrinks the allocation of the heap to twice its length if it is currently more
than 75% empty. Does nothing otherwise. This is the only way to shrink the
allocation of the heap–it never shrinks itself automatically.

#### mm_len
```
size_t mm_len(minmax *m)
```
Returns the number of elements in the heap.

#### mm_insert
```
void mm_insert(minmax *m, type T, T elt)
```
Inserts an element into the heap. Triggers a `realloc` if the heap is already
at full capacity.

#### mm_peekmin / mm_peekmax
```
bool mm_peekmin(minmax *m, type T, T elt)

bool mm_peekmax(minmax *m, type T, T elt)
```
Stores either the minimum or maximum element in the heap in `elt` or returns
`false` of the heap is empty.

#### mm_pollmin / mm_pollmax
```
bool mm_pollmin(minmax *m, type T, T elt)

bool mm_pollmax(minmax *m, type T, T elt)
```
Removes either the minimum or maximum element from the heap and stores it in
`elt` or returns `false` of the heap is empty.

### Notes
This library reserves the "namespaces" `mm_`, `minmax_`, `MM_`, and `MINMAX_`.
