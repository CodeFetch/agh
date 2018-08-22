#!/bin/sh

# not yet working glib stuff
G_MESSAGES_DEBUG=all
export G_MESSAGES_DEBUG

# mips
STAGING_DIR=/mnt/hdd/sdata/x86openwrt/staging_dir/toolchain-x86_64_gcc-7.3.0_glibc/
export STAGING_DIR
PATH=$PATH:$STAGING_DIR/bin
export PATH

PKG_CONFIG_LIBDIR=/mnt/hdd/sdata/x86openwrt/staging_dir/target-x86_64_glibc/usr/lib/pkgconfig
PKG_CONFIG_SYSROOT_DIR=/mnt/hdd/sdata/x86openwrt/staging_dir/target-x86_64_glibc/
export PKG_CONFIG_LIBDIR
export PKG_CONFIG_SYSROOT_DIR

x86_64-openwrt-linux-gcc -Wall -Wextra -ggdb -Wno-unused-variable -Wno-unused-parameter \
agh.c \
agh_handlers.c \
agh_xmpp.c \
agh_messages.c \
agh_commands.c \
agh_xmpp_handlers.c \
agh_modem.c \
agh_mm_handlers.c \
agh_mm_helpers.c \
agh_ubus.c \
agh_ubus_handler.c \
agh_ubus_helpers.c \
agh_ubus_logstream.c \
agh_modem_config.c \
agh_xmpp_caps.c \
agh_mm_manager.c \
agh_mm_sm.c \
agh_mm_helpers_sm.c \
`pkg-config --cflags --libs glib-2.0 libconfig gio-2.0 mm-glib nettle` -lubus -I $PKG_CONFIG_SYSROOT_DIR/usr/include/ \
-Wl,-L/mnt/hdd/sdata/x86openwrt/staging_dir/target-x86_64_glibc/usr/lib/ \
-Wl,-rpath-link=/mnt/hdd/sdata/x86openwrt/staging_dir/target-x86_64_glibc/usr/lib/ \
-lstrophe -lblobmsg_json -lubox -luci -DG_DISABLE_DEPRECATED \
-o agh || exit 1

#pkg-config --cflags --libs glib-2.0
ssh -p 2200 root@localhost killall -9 agh
ssh -p 2200 root@localhost rm -rf /tmp/agh
scp -r -q -oPort=2200 /tmp/agh/ root@localhost:/tmp/
