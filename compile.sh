#!/bin/sh
G_MESSAGES_DEBUG=all
export G_MESSAGES_DEBUG
#PKG_CONFIG_PATH=$(pwd)/debug/lib/pkgconfig:/usr/lib/pkgconfig/ 
#export PKG_CONFIG_PATH

gcc -Wall -Wextra -ggdb -Wno-unused-variable -Wno-unused-parameter \
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
agh_xmpp_caps.c \
agh_mm_manager.c \
`pkg-config --cflags --libs glib-2.0 libstrophe libconfig gio-2.0 mm-glib nettle` -lubus -lblobmsg_json -lubox -luci -DG_DISABLE_DEPRECATED \
-o agh
