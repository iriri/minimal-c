# minimal
Bad single header libraries.

## slice.h (C11)
Slices without preprocessor cancer. Both arrays and slices can be sliced with
the same macros thanks to C11 generic selection. Not safe for concurrent use.
Somewhat tested and currently being dogfooded. See `slice_test.c` for
information about usage.

## slice.h (GNU99)
A slightly different approach to slices using GNU extensions. Only GCC and
Clang have been tested (be sure to compile with `-Wno-language-extension-token`
if using Clang). Mostly nicer to use than the C11 version but not as tested or
as dogfooded (yet). Pointer types require a PTR_OF wrapper during slice
creation, CONCAT/SLICE_FROM are no longer polymorphic with respect to slices
and slice pointers/slices and arrays, and SLICE_OF no longer supports arrays
but all of these are very minor. In exchange, macros that do not create a slice
(and even some macros that do) no longer require us to provide a type as an
argument, saving a lot of typing and giving us much more type safety as we are
no longer casting everything. Performance is similar between the two versions.
See `slice_test.c` for information about usage.

## ringbuf.h (GNU99)
A bounded concurrent MPMC queue in 99 lines. Uses GNU extensions in the same
manner as the GNU99 version of `slice.h`. Push returns false if the queue is
now full and pop returns false if queue is currently empty. If pushed to when
full, the oldest element is overwritten. Not at all well tested. Not really
designed to be used for synchronization but it's possible to spin on trypush
and pop if you really want to (e.g. to ensure determinism for testing). See
`ringbuf_test.h` for information about usage.

## channel.h (GNU99)
Not done; not tested; don't use.
