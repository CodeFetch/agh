#!/bin/sh

rm -f valgrind.out
G_SLICE=always-malloc G_DEBUG=gc-friendly valgrind \
--suppressions=../glib.supp \
--leak-check=full \
--leak-resolution=high \
--show-reachable=no \
--log-file=valgrind.out \
--expensive-definedness-checks=yes \
$@
