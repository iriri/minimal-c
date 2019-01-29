## channel.h
This library provides an implementation of a Go-style channel, a.k.a. MPMC
blocking bounded queue with support for multiplexing. Changes have been made
from Go's channel design to improve the multi-producer use case and reduce
reliance on a `select`-style statement. The buffered channel fast path is
lock-free.

Support for POSIX threads, due to lack of widespread support for C11 threads,
and either POSIX or libdispatch semaphores is required. Recent versions of
Linux, OS X, and Cygwin have all been tested successfully. Support for C11 is
required for `stdatomic.h` but if some other implementation of atomic variables
is substituted in, C99 support should be good enough.

### Types
```
typedef union channel channel;
typedef struct channel_set channel_set;
typedef uint32_t channel_rc;
typedef enum channel_op channel_op;
```

### Values
#### Return codes
```
#define CH_OK 0
#define CH_WBLOCK (UINT32_MAX - 1)
#define CH_CLOSED UINT32_MAX
```

#### Op codes
```
enum channel_op {
    CH_NOOP,
    CH_SEND,
    CH_RECV,
};
```

### Functions
#### ch_make / ch_dup / ch_drop
```
channel *ch_make(type T, uint32_t cap)
channel *ch_dup(channel *c)
channel *ch_drop(channel *c)
```
`ch_make` allocates and initializes a new channel. If the capacity is 0 then
the channel is unbuffered. Otherwise the channel is buffered.

`ch_dup` increments the reference count of the channel and returns the channel.

`ch_drop` deallocates all resources associated with the channel if the caller
has the last reference. Decrements the reference count otherwise. Returns
`NULL`.

#### ch open / ch_close
```
channel *ch_open(channel *c)
channel *ch_close(channel *c)
```
`ch_open` increments the open count of the channel and returns the channel.
Additional opens can be useful in scenarios with multiple producers
independently closing the channel.

`ch_close` closes the channel if the caller has the last open handle to the
channel. Decrements the open count otherwise. Returns the channel.

#### ch_send / ch_recv
```
channel_rc ch_send(channel *c, T msg)
channel_rc ch_recv(channel *c, T msg)
```
Blocking sends and receives block on buffered channels if the buffer is full or
empty, respectively, and block on unbuffered channels if there is no waiting
receiver or sender, respectively. Both return `CH_OK` on success or `CH_CLOSED`
if the channel is closed.

#### ch_trysend / ch_tryrecv
```
channel_rc ch_trysend(channel *c, T msg)
channel_rc ch_tryrecv(channel *c, T msg)
```
Nonblocking sends and receives fail on buffered channels if the channel is full
or empty, respectively, and fail on unbuffered channels if there is no waiting
receiver or sender, respectively. Both return `CH_OK` on success, `CH_WBLOCK`
on failure, or `CH_CLOSED` if the channel is closed.

#### ch_timedsend / ch_timedrecv
```
channel_rc ch_timedsend(channel *c, T msg, uint64_t timeout)
channel_rc ch_timedrecv(channel *c, T msg, uint64_t timeout)
```
Timed sends and receives fail on buffered channels if the channel is full or
empty, respectively, for the duration of the timeout, and fail on unbuffered
channels if there is no waiting receiver or sender, respectively, for the
duration of the timeout. The timeout is specified in microseconds. Both return
`CH_OK` on success, `CH_WBLOCK` on failure, or `CH_CLOSED` if the channel is
closed.

Not very well tested.

#### ch_set_make / ch_set_drop
```
ch_set *ch_set_make(uint32_t cap)
ch_set *ch_set_drop(ch_set *s)
```
`ch_set_make` allocates and initializes a new channel set for use with
`ch_select`. `cap` is not a hard limit but a `realloc` must be done every time
it grows past the current capacity.

`ch_set_drop` deallocates all resources associated with the channel set and
returns `NULL`.

#### ch_set_add / ch_set_rereg
```
uint32_t ch_set_add(ch_set *s, channel *c, ch_op op, T msg)
void ch_set_rereg(ch_set *s, uint32_t id, ch_op op, T msg)
```
`ch_set_add` adds a channel to the channel set and registers the specified
operation and element.

`ch_set_rereg` changes the operation and/or element registered to the specified
channel in the channel set.

#### ch_select
```
uint32_t ch_select(channel_set *s)
uint32_t ch_tryselect(channel_set *s)
uint32_t ch_select(channel_set *s, uint64_t timeout)
```
`ch_select` blocks indefinitely until one of the registered operations in the
channel set can be completed.

`ch_tryselect` attempts to complete an operation or returns immediately with
`CH_WBLOCK` if it fails to do so.

`ch_timedselect` attempts to complete an operation before the timeout,
specified in microseconds, expires or returns with `CH_WBLOCK` if it fails to
do so.

If multiple operations can be completed, just one is chosen at random. All
three return the id of the channel that completed its operation upon success
and return `CH_CLOSED` if all of the channels in the set are either closed or
registered with `CH_NOOP`.

Not very well tested.

#### ch_poll / ch_case / ch_default / ch_poll_end
```
ch_poll(int casec)
ch_case(int id, int (*fn)(channel *, T), { block })
ch_default({ block })
ch_poll_end
```
ch_poll is an alternative to `ch_select`. It continuously loops over the cases
(`fn` is intended to be either `ch_trysend` or `ch_tryrecv`) rather than
blocking, which may be preferable in cases where one of the operations is
expected to succeed or if there is a default case but burns a lot of cycles
otherwise. See `tests/basic.c` for an example.

### Notes
This library reserves the "namespaces" `ch_`, `channel_`, `CH_`, and
`CHANNEL_`.

The `ch_send` family of functions does not support sending literals. This may
be supported in the future by a separate version that uses GNU extensions.
