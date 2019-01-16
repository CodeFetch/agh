#!/bin/sh

rm -f valgrind.out
G_SLICE=always-malloc valgrind \
--suppressions=../valgrind.suppressions \
--leak-check=full \
--leak-resolution=high \
--show-reachable=no \
--log-file=valgrind.out \
$@
