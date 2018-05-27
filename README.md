# minimal
Bad single header libraries.

### Goals
Simplicity - Libraries should be one header, require minimal boilerplate, and
store values, not references. Interfaces should be concise. User simplicity
should be favored over both implementation simplicity and performance.

Safety - Type checking information should be preserved for the compiler in
libraries that use GNU extensions. For pragmatic reasons, this requirement is
relaxed for libraries that are restricted to standard C but mangling data
structures, overrunning buffers, etc. should always be prevented. Tests should
compile cleanly with `-Wall -Wextra -Wpedantic -O2` (for reasonable values of
`-Wextra` and the relevant `-std`) and run cleanly with the relevant
sanitizers. The "strict-aliasing" rule should not be violated.

## channel.h
Go-style channel—MPMC blocking bounded queue with support for multiplexing.
The buffered channel fast path is lock-free. See `channel/README.md` for
documentation.

## minmax.h
Min-max-heap-based double-ended priority queue. Not really done and not really
tested much.

## vector.h
Conventional array-based vector that also supports stack operations. Both GNU99
and C99 versions are available. Not really tested at all yet tbh. See
`vector/README.md` for an overview of the differences.
