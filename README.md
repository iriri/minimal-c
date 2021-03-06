# minimal
Bad single header libraries.

### Goals
Simplicity - Libraries should be one header, require minimal boilerplate, and
store values, not references. Interfaces should be concise. User simplicity
should be favored over both implementation simplicity and performance.

Safety - Mangling data structures, overrunning buffers, etc. should always be
prevented. Tests should compile cleanly with `-Wall -Wextra -Wpedantic -O2`
(for reasonable values of `-Wextra` and the relevant `-std`) and run cleanly
with the relevant sanitizers. The "strict-aliasing" rule should not be
violated.

## channel.h
Go-style channel, a.k.a. MPMC blocking bounded queue with support for
multiplexing. The buffered channel fast path is lock-free. Somewhat tested and
poorly fuzzed. See `channel/README.md` for documentation.

## minmax.h
Min-max-heap-based double-ended priority queue. Somewhat tested and fairly well
fuzzed, given that there really isn't too much you can do with it. See
`minmax/README.md` for documentation.

## vector.h
Conventional array-based vector that also supports stack operations. Somewhat
tested. See `vector/README.md` for documentation.
