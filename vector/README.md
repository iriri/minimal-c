## Differences between the GNU99 and C99 versions

The GNU99 version preserves full type checking information for the compiler
while the C99 version can only perform very crude size checks at runtime.

The GNU99 version requires the user to define individual vector types by
invoking a preprocessor macro for each type. Many of the functions in the C99
version require the type of the vector as an additional parameter.

The C99 version allocates `sizeof(size_t)` more memory per vector.
