#!/bin/sh

# not yet working glib stuff
G_MESSAGES_DEBUG=all
export G_MESSAGES_DEBUG

# mips
STAGING_DIR=/mnt/hdd/sdata/openwrt/staging_dir/toolchain-mipsel_74kc_gcc-7.3.0_musl/
export STAGING_DIR
PATH=$PATH:$STAGING_DIR/bin
export PATH

PKG_CONFIG_LIBDIR=/mnt/hdd/sdata/openwrt/staging_dir/target-mipsel_74kc_musl/usr/lib/pkgconfig
PKG_CONFIG_SYSROOT_DIR=/mnt/hdd/sdata/openwrt/staging_dir/target-mipsel_74kc_musl/
export PKG_CONFIG_LIBDIR
export PKG_CONFIG_SYSROOT_DIR

mipsel-openwrt-linux-gcc -Wall -Wextra -ggdb -Wno-unused-variable -Wno-unused-parameter \
agh.c \
handlers.c \
xmpp.c \
aghservices.c \
messages.c \
commands.c \
xmpp_handlers.c \
modem.c \
modem_handlers.c \
`pkg-config --cflags --libs glib-2.0 libconfig gio-2.0 mm-glib` -I $PKG_CONFIG_SYSROOT_DIR/usr/include/ \
-Wl,-L/mnt/hdd/sdata/openwrt/staging_dir/target-mipsel_74kc_musl/usr/lib/ \
-Wl,-rpath-link=/mnt/hdd/sdata/openwrt/staging_dir/target-mipsel_74kc_musl/usr/lib/ \
-lstrophe -DG_DISABLE_DEPRECATED \
-o agh || exit 1

#pkg-config --cflags --libs glib-2.0
ssh root@192.168.0.110 killall -9 agh
ssh root@192.168.0.110 rm /tmp/agh
scp agh root@192.168.0.110:/tmp/
