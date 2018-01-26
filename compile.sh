#!/bin/sh
G_MESSAGES_DEBUG=all
export G_MESSAGES_DEBUG

gcc -Wall -Wextra -ggdb \
agh.c \
xmpp.c \
callbacks.c \
`pkg-config --cflags --libs glib-2.0` -DG_DISABLE_DEPRECATED -o agh
