## vector.h
This library provides an implementation of an array-based vector. Stack
semantics can be achieved by exclusively using `vec_push`, `vec_peek`, and
`vec_pop`.

Support for C99 and the GNU extensions `__COUNTER__`, statement expressions,
labeled gotos, `__auto_type`, and `typeof` is required.

### Definitions
```
#define VECTOR_DEF(T)
#define VECTOR_DEF_PTR(T)
```
Each individual vector type must be defined before use. Pointer types must be
wrapped with `ptr` instead of using `*` as type names are created via token
pasting. E.g. `VECTOR_DEF_PTR(int);` defines the type of vectors of pointers to
integers and `vector(ptr(int)) v;` declares one such vector.

### Types
```
typedef union vector(T) {
    T *arr;
    ...
} vector(T);
typedef T *ptr(T)
```

### Functions
#### vec_make / vec_drop
```
vector(T) *vec_make(type T, size_t len, size_t cap)
vector(T) *vec_drop(vector(T) *v)
```
`vec_make` allocates and initializes a new vector with the specified initial
length and capacity.

`vec_drop` deallocates all resources associated with the vector and returns
`NULL`.

#### vec_shrink / vec_trim
```
void vec_shrink(vector(T) *v)
void vec_trim(vector(T) *v)
```
`vec_shrink` shrinks the allocation of the vector to twice its length if it is
currently more than 75% empty. It does nothing otherwise. `vec_trim` shrinks
the allocation of the vector to exactly its length. These are the only way to
shrink the allocation of the vector–it never shrinks itself automatically.

#### vec_len
```
size_t vec_len(vector(T) *v)
```
Returns the number of elements in the vector.

#### vec_arr / vec_index
```
T[] vec_arr(vector(T) *v)
T vec_index(vector(T) *v, size_t index)
```
`vec_arr` provides direct access to the backing array of the vector and should
be preferred when bounds checking is not required.

`vec_index` provides access to the specified index of the vector as an lvalue
and is bounds checked.

#### vec_push
```
void vec_push(vector(T) *v, T elt)
```
Appends `elt` to the end of the vector. Triggers a `realloc` if the vector is
already at full capacity.

#### vec_peek / vec_pop
```
bool vec_peek(vector(T) *v, T elt)
bool vec_pop(vector(T) *v, T elt)
```
`vec_peek` stores the last element of the vector in `elt` or returns `false` if
the vector is empty.

`vec_pop` removes the last element of the vector and stores it in `elt` or
returns `false` if the vector is empty.

#### vec_concat
```
void vec_concat(vector(T) *v, vector(T) *v1)
```
Concatenates `v` and `v1`. `v` *can* alias `v1`.

#### vec_find
```
size_t vec_find(vector(T) *v, T elt)
```
Returns the index of the first element in the vector that is equal to `elt`, as
defined by the `==` operator. This may not (or may) have the desired behavior
when comparing floating point or complex numbers.

#### vec_remove
```
void vec_remove(vector(T) *v, size_t index)
```
Removes the element at `index` from the vector.

### Notes
This library defines the macros `ptr` and `PTR_DEF`, and reserves the
"namespaces" `vec_`, `vector_`, `VEC_`, and `VECTOR_`.
