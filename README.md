# minimal
Bad single header libraries. Mostly implemented via mild preprocessor cancer
and light GNU extension abuse. Only GCC and Clang have been tested so far.

## channel.h (GNU99)
Go-style channels with some minor changes to make the multi-producer use case
more pleasant. `ch_select` is kind of terrible right now but nonblocking sends
and receives can be done by just using `ch_trysend` and `ch_tryrecv` outside of
`ch_select`. (As `ch_select` is just a thinly veiled Duff's Device you'll need
to compile with `-Wno-implicit-fallthrough` to avoid warnings from GCC.) Not
really tested very well yet. See `channel_test.c` for information about usage.

## slice.h (GNU99)
A sliceable vector type. Not safe for concurrent use. Somewhat tested. See
`slice_test.c` for information about usage.

## slice.h (C11)
Slices without preprocessor cancer. Both arrays and slices can be sliced with
the same macros thanks to C11 generic selection. Not safe for concurrent use.
Somewhat tested. See `slice_test.c` for information about usage.

## ringbuf.h (GNU99)
A bounded concurrent MPMC queue in 99 lines. Not at all well tested. Not really
designed to be used for synchronization but it's possible to spin on
`rbuf_trypush` and `rbuf_shift` if you really want to burn some cycles (e.g. to
ensure determinism for testing). See `ringbuf_test.h` for information about
usage.
