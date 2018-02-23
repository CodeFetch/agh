#!/bin/sh
G_MESSAGES_DEBUG=all
export G_MESSAGES_DEBUG
#PKG_CONFIG_PATH=$(pwd)/debug/lib/pkgconfig:/usr/lib/pkgconfig/ 
#export PKG_CONFIG_PATH

gcc -Wall -Wextra -ggdb -Wno-unused-variable -Wno-unused-parameter \
agh.c \
handlers.c \
xmpp.c \
aghservices.c \
messages.c \
xmpp_handlers.c \
`pkg-config --cflags --libs glib-2.0 libstrophe` -DG_DISABLE_DEPRECATED \
-o agh
