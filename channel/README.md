## channel.h
This library implements Go-style channels—MPMC blocking bounded queues with
support for multiplexing. Changes have been made from Go's channel design to
improve the multi-producer use case and reduce reliance on `select`. The
buffered channel fast path is lock-free. C11 support is required for
`stdatomic.h`.

TODO: Send, receive, and select with timeouts.

### Types
```
typedef union channel channel;
typedef struct channel_set channel_set;
typedef enum channel_op channel_op;
```

### Values
#### Return codes
```
#define CH_OK 0x0
#define CH_CLOSED 0x1
#define CH_FULL 0x2
#define CH_EMPTY 0x4
#define CH_TIMEDOUT 0x8

#define CH_SEL_CLOSED UINT32_MAX
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
#### ch_make / ch_drop
```
channel *ch_make(type T, uint32_t cap)

channel *ch_drop(channel *c)
```
`ch_make` allocates and initializes a new channel. If the capacity is 0 then
the channel is unbuffered. Otherwise the channel is buffered.

`ch_drop` deallocates all resources associated with the channel and returns
`NULL`.

#### ch_dup
```
channel *ch_dup(channel *c)
```
Increases the reference count of the channel and returns it. Intended to be
used with multiple producers that independently close the channel.

#### ch_close
```
void ch_close(channel *c)
```
Closes the channel if the caller has the last reference to the channel.
Decreases the reference count otherwise. Attempting to send on a closed channel
immediately returns an error code. Receiving on a closed channel succeeds until
the channel is emptied.

#### ch_send / ch_recv
```
int ch_send(channel *c, T elt)

int ch_recv(channel *c, T elt)
```
Blocking sends and receives block on buffered channels if the buffer is full or
empty, respectively, and block on unbuffered channels if there is no waiting
receiver or sender, respectively. Both return `CH_OK` on success or `CH_CLOSED`
if the channel is closed.

#### ch_trysend / ch_tryrecv
```
int ch_trysend(channel *c, T elt)

int ch_tryrecv(channel *c, T elt)
```
Nonblocking sends and receives fail on buffered channels if the channel is full
or empty, respectively, and fail on unbuffered channels if there is no waiting
receiver or sender, respectively. Both return `CH_OK` on success or `CH_CLOSED`
if the channel is closed. `ch_trysend` and `ch_tryrecv` respectively return
`CH_FULL` and `CH_EMPTY` on failure.

#### ch_forcesend
```
int ch_forcesend(channel *c_, T elt)
```
Forced sends on buffered channels do not block and instead overwrite the oldest
message if the buffer is full. Forced sends are not possible with unbuffered
channels. Returns `CH_OK` on success or `CH_CLOSED` if the channel is closed.

Not well tested.

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
uint32_t ch_set_add(ch_set *s, channel *c, ch_op op, T elt)

void ch_set_rereg(ch_set *s, uint32_t id, ch_op op, T elt)
```
`ch_set_add` adds a channel to the channel set and registers the specified
operation and element.

`ch_set_rereg` changes the operation and/or element registered to the specified
channel in the channel set.

#### ch_select
```
uint32_t ch_select(channel_set *s)
```
Randomly performs the registered operation on one channel in the channel set
that is ready to perform that operation. Blocks if no channel is ready.
Returns the id of the channel successfully completes its operation or
`CH_SEL_CLOSED` if all channels are closed or have been registered with
`CH_NOOP`.

Not well tested.

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
This library reserves the "namespaces" `channel_`, `ch_`, and `CH_`.

Cache line padding is disabled by default. It can be enabled by defining
`CH_PAD_CACHE_LINES`.

The `ch_send` family of functions does not support sending literals. This may
be supported in the future by a separate version that uses GNU extensions.
