libcheck normally runs each test in memory isolation,
but for gdb, valgrind and gcov we need to run them, with the
nofork option.

Therefore all tests must be tested using both:
`make test` and `make test_nofork`.

Valgrind can be run with `make valgrind`.

LCOV reports can be generated using `make lcov`.

Scan-build can be built using `make scan-build`.
