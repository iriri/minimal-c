## channel.h
This library implements Go-style channels—MPMC blocking bounded queues with
support for multiplexing. Changes have been made from Go's channel design to
improve the multi-producer use case and reduce reliance on `select`. Buffered
channels (queues with capacity of at least 1) are lock-free in the fast path.
Currently requires C11; this may change in the future to either GNU99 (in which
case a C11 version will still be maintained) or C99.

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
#### ch_make
```
channel *ch_make(size_t eltsize, uint32_t cap)
```
Allocates a new channel. If the capacity is 0 then the channel is unbuffered.
Otherwise the channel is buffered. Returns NULL.

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

#### ch_drop
```
channel *ch_drop(channel *c)
```
Deallocates all resources associated with the channel.

#### ch_send
```
int ch_send(channel *c, T elt)
```
Blocking sends block on buffered channels if the buffer is full and block on
unbuffered channels if there is no waiting receiver. Returns `CH_OK` on success
or `CH_CLOSED` if the channel is closed.

#### ch_trysend
```
int ch_trysend(channel *c, T elt)
```
Nonblocking sends fail on buffered channels if the channel is full and fail on
unbuffered channels if there is no waiting receiver. Returns `CH_OK` on
success, `CH_FULL` on failure, or `CH_CLOSED` if the channel is closed.

#### ch_forcesend
```
int ch_forcesend(channel *c_, T elt)
```
Forced sends on buffered channels do not block and overwrite the oldest message
if the buffer is full. Forced sends are not possible with unbuffered channels.
Returns `CH_OK` on success or `CH_CLOSED` if the channel is closed.

Not well tested.

#### ch_recv
```
int ch_recv(channel *c, T elt)
```
Blocking receives block on buffered channels if the buffer is empty and
block on unbuffered channels if there is no waiting sender. Returns `CH_OK`
on success or `CH_CLOSED` if the channel is closed.

#### ch_tryrecv
```
int ch_tryrecv(channel *c, T elt)
```
Nonblocking receives fail on buffered channels if the channel is empty and fail
on unbuffered channels if there is no waiting sender. Returns `CH_OK` on
success, `CH_EMPTY` on failure, or `CH_CLOSED` if the channel is closed.

#### ch_set_make
```
ch_set *ch_set_make(uint32_t cap)
```
Allocates a new channel set. The cap is not a hard limit but a realloc must be
done every time it must grow past the current capacity.

#### ch_set_drop
```
ch_set *ch_set_drop(ch_set *s) {
```
Deallocates all resources associated with the channel set. Returns NULL.

#### ch_set_add
```
uint32_t ch_set_add(ch_set *s, channel *c, ch_op op, T elt)
```
Adds a channel to the channel set and registers the specified operation and
element.

#### ch_set_rereg
```
void ch_set_rereg(ch_set *s, uint32_t id, ch_op op, T elt)
```
Change the registered op or element of the specified channel in the channel
set.

#### ch_select
```
inline uint32_t ch_select(channel_set *s)
```
Randomly performs the registered operation on one channel in the channel set
that is ready to perform that operation. Blocks if no channel is ready.
Returns the id of the channel successfully completes its operation or
`CH_SEL_CLOSED` if all channels are closed or have been registered with
`CH_NOOP`.

Not well tested.
