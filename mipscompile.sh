#!/bin/sh

# not yet working glib stuff
G_MESSAGES_DEBUG=all
export G_MESSAGES_DEBUG

# mips
STAGING_DIR=/home/mrkiko/files/src/openwrt/staging_dir/toolchain-mipsel_74kc_gcc-7.3.0_musl/
export STAGING_DIR
PATH=$PATH:$STAGING_DIR/bin
export PATH

PKG_CONFIG_LIBDIR=/home/mrkiko/files/src/openwrt/staging_dir/target-mipsel_74kc_musl/usr/lib/pkgconfig
PKG_CONFIG_SYSROOT_DIR=/home/mrkiko/files/src/openwrt/staging_dir/target-mipsel_74kc_musl/
export PKG_CONFIG_LIBDIR
export PKG_CONFIG_SYSROOT_DIR

mipsel-openwrt-linux-gcc -Wall -Wextra -ggdb -Wno-unused-variable -Wno-unused-parameter \
agh.c \
handlers.c \
xmpp.c \
aghservices.c \
messages.c \
xmpp_handlers.c \
`pkg-config --cflags --libs glib-2.0` -I $PKG_CONFIG_SYSROOT_DIR/usr/include/ \
-Wl,-L/home/mrkiko/files/src/openwrt/staging_dir/target-mipsel_74kc_musl/usr/lib/ \
-Wl,-rpath-link=/home/mrkiko/files/src/openwrt/staging_dir/target-mipsel_74kc_musl/usr/lib/ \
-lstrophe -DG_DISABLE_DEPRECATED \
-o agh

#pkg-config --cflags --libs glib-2.0