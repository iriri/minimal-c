# minimal
This is currently a dumping ground for (bad) ideas and will eventually be
cleaned up, maybe.

~Bad single header libraries. The headers found in the top level all (ab)use
GNU extensions and should compile without warnings with `-Wall -Wextra
-Wpedantic -std=gnu99`, except when noted. The headers found in the `c11`
directory should all compile without warnings with `-Wall -Wextra -Wpedantic`
and either `-std=c99` or `-std=c11` depending on the features used. Only GCC
and Clang have been tested so far.~

## ~channel.h~
~Go-style channels with some minor changes to make the multi-producer use case
more pleasant. There are currently two very experimental (i.e. incomplete and
completely broken) implementations of channel multiplexing, both of which are
less powerful than Go's `select`: `ch_poll` is essentially a glorified Duff's
Device (so you'll have to compile with `-Wno-implicit-fallthrough` to avoid
warnings from GCC), while `ch_select` aims to better support extended waits via
condition variables but channels can only be part of one `ch_select` at at
time. Nonblocking sends and receives are available via `ch_trysend` and
`ch_tryrecv` and timed sends and receives are available via `ch_timedsend` and
`ch_timedrecv`. However, the correctness of the behavior of the latter pair is
dependent on the quality of the target platform's pthreads implementation. The
C99 and GNU99 variants are almost identical but the former is much more
readable as it uses standard functions while the latter consists entirely of
preprocessor macros. Thus the C99 version, which also includes light
documentation in the form of comments, should be used as a reference even if
using the GNU99 version. (`ch_select` is currently not availble in the GNU99
variant.)~

## ~slice.h~
~A sliceable vector type. Not safe for concurrent use. Somewhat tested. See
`slice_test.c` for information about usage.~

## ~ringbuf.h~
~A bounded concurrent MPMC queue in 99 lines. Not at all well tested. Not
really designed to be used for synchronization but it's possible to spin on
`rbuf_trypush` and `rbuf_shift` if you really want to burn some cycles (e.g. to
ensure determinism for testing). Don't use this.~
