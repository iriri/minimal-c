# minimal
Bad single header libraries. The headers found in the top level all (ab)use GNU
extensions and should compile without warnings with `-Wall -Wextra -Wpedantic
-std=gnu99`, except when noted. The headers found in the `c11` directory should
all compile without warnings with `-Wall -Wextra -Wpedantic` and either
`-std=c99` or `-std=c11` depending on the features used. Only GCC and Clang
have been tested so far.

## channel.h
Go-style channels with some minor changes to make the multi-producer use case
more pleasant. `ch_select` is kind of terrible right now but nonblocking sends
and receives can be done by just using `ch_trysend` and `ch_tryrecv` outside of
`ch_select`. `ch_timedsend` and `ch_timedrecv` are available as well, although
the correctness of their behavior is dependent on the quality of the target
platform's pthreads implementation. (As `ch_select` is just a thinly veiled
Duff's Device you'll need to compile with `-Wno-implicit-fallthrough` to avoid
warnings from GCC.) The C99 and GNU99 variations are almost identical but the
former is much more readable as it uses standard functions while the latter
consists entirely of preprocessor macros. Thus the C99 version, which also
includes light documentation in the form of comments, should be used as a
reference even if using the GNU99 version.

## slice.h
A sliceable vector type. Not safe for concurrent use. Somewhat tested. See
`slice_test.c` for information about usage.

## ringbuf.h
A bounded concurrent MPMC queue in 99 lines. Not at all well tested. Not really
designed to be used for synchronization but it's possible to spin on
`rbuf_trypush` and `rbuf_shift` if you really want to burn some cycles (e.g. to
ensure determinism for testing). Don't use this.
