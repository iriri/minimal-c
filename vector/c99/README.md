## vector.h
This library provides an implementation of an array-based vector. Stack
semantics can be achieved by exclusively using `vec_push`, `vec_peek`, and
`vec_pop`.

Support for C99 is required.

### Types
```
typedef struct vector vector;
```

### Functions
#### vec_make / vec_drop
```
vector *vec_make(type T, size_t len, size_t cap)
vector *vec_drop(vector *v)
```
`vec_make` allocates and initializes a new vector with the specified initial
length and capacity.

`vec_drop` deallocates all resources associated with the vector and returns
`NULL`.

#### vec_shrink / vec_trim
```
void vec_shrink(vector *v)
void vec_trim(vector *v)
```
`vec_shrink` shrinks the allocation of the vector to twice its length if it is
currently more than 75% empty. It does nothing otherwise. `vec_trim` shrinks
the allocation of the vector to exactly its length. These are the only way to
shrink the allocation of the vector–it never shrinks itself automatically.

#### vec_len
```
size_t vec_len(vector *v)
```
Returns the number of elements in the vector.

#### vec_arr / vec_index
```
T[] vec_arr(vector *v, type T)
T vec_index(vector *v, type T, size_t index)
```
`vec_arr` provides direct access to the backing array of the vector and should
be preferred when bounds checking is not required.

`vec_index` provides access to the specified index of the vector as an lvalue
and is bounds checked.

#### vec_push
```
void vec_push(vector *v, type T, T elt)
```
Appends `elt` to the end of the vector. Triggers a `realloc` if the vector is
already at full capacity.

#### vec_peek / vec_pop
```
bool vec_peek(vector *v, type T, T elt)
bool vec_pop(vector *v, type T, T elt)
```
`vec_peek` stores the last element of the vector in `elt` or returns `false` if
the vector is empty.

`vec_pop` removes the last element of the vector and stores it in `elt` or
returns `false` if the vector is empty.

#### vec_concat
```
void vec_concat(vector *v, vector *v1)
```
Concatenates `v` and `v1`. `v` *can* alias `v1`.

#### vec_find
```
size_t vec_find(vector *v, T elt)
```
Returns the index of the first element in the vector that is bitwise equal to
`elt`. This may not (or may) have the desired behavior when comparing floating
point or complex numbers, or structs that have padding.

#### vec_remove
```
void vec_remove(vector *v, size_t index)
```
Removes the element at `index` from the vector.

### Notes
This library reserves the "namespaces" `vec_`, `vector_`, `VEC_`, and
`VECTOR_`.
