# minimal
Bad single header libraries.

## minimal/slice (C11)
Slices ~~in 100 lines~~. Both arrays and slices can be sliced with the same
macros thanks to C11 generic selection. Not safe for concurrent use. Somewhat
tested and currently being dogfooded. See `slice_test.c` for information about
usage.
