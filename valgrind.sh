#!/bin/sh

rm -f valgrind.out
G_SLICE=always-malloc valgrind --suppressions=valgrind.suppressions --leak-check=full --show-reachable=no --log-file=valgrind.out ./agh

#G_SLICE=always-malloc valgrind --suppressions=valgrind.suppressions --leak-check=full --show-reachable=yes $#
#G_SLICE=debug-blocks valgrind --tool=memcheck --leak-check=full --show-reachable=yes --suppressions=valgrind.suppressions $@
#G_SLICE=always-malloc valgrind --tool=massif --detailed-freq=2 --max-snapshots=400 --num-callers=20 $@
#G_SLICE=gc-friendly valgrind --tool=memcheck --leak-check=full --show-reachable=yes $@
