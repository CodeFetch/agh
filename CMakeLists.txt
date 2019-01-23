CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

PROJECT(AGH C)
ADD_DEFINITIONS(-Wall -Wextra -Wno-unused-variable -Wno-unused-parameter)

FIND_PACKAGE(PkgConfig REQUIRED)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

PKG_SEARCH_MODULE(GLIB REQUIRED glib-2.0>=2.56.0)
PKG_SEARCH_MODULE(LIBSTROPHE REQUIRED libstrophe>=0.9.2)
PKG_SEARCH_MODULE(LIBCONFIG REQUIRED libconfig>=1.7.2)
PKG_SEARCH_MODULE(GIO REQUIRED gio-2.0)
PKG_SEARCH_MODULE(MM-GLIB REQUIRED mm-glib>=1.8.2)
PKG_SEARCH_MODULE(NETTLE REQUIRED nettle>=3.4.1)

FIND_PATH(ubus_include_dir libubus.h)
INCLUDE_DIRECTORIES(${ubus_include_dir})

FIND_PATH(ubox_include_dir libubox/blobmsg_json.h)
INCLUDE_DIRECTORIES(${ubox_include_dir})

SET(SOURCES
	# AGH core: infrastructure (e.g.: commands, handlers, messages...)
	agh.c agh_commands.c agh_handlers.c agh_messages.c agh_logging.c

	# Modem: connecting to ModemManager via D-Bus (e.g.: contains agh_mm_init)
	agh_modem.c agh_mm_manager.c

	# Modem: ModemManager general helpers
	agh_mm_helpers.c

	# Modem: configuration related stuff
	agh_modem_config.c

	# Modem: state machine for cellular connection handling
	agh_mm_sm.c

	# and state machine helper
	agh_mm_helpers_sm.c

	# Modem: AGH handler (e.g.: responds to the "modem" command)
	agh_mm_handlers.c

	# ubus: connection maintenance...
	agh_ubus.c

	# ubus related helpers
	agh_ubus_helpers.c

	# really contains code to interact with OpenWrt's ubox package log daemon, logd
	agh_ubus_logstream.c

	# ubus: handler, responds to the "ubus" command
	agh_ubus_handler.c

	# XMPP code
	agh_xmpp.c

	# some code to handle XMPP capabilities
	agh_xmpp_caps.c

	# XMPP handlers; contains also code involved in sending out messages
	agh_xmpp_handlers.c
)

SET(LIBS
	ubox ubus uci json-c blobmsg_json)

IF(DEBUG)
  ADD_DEFINITIONS(-g3 -ggdb)
  IF(NO_OPTIMIZE)
    ADD_DEFINITIONS(-O0)
  ENDIF()
ENDIF()

ADD_EXECUTABLE(agh ${SOURCES} ${GLIB_LIBRARY} ${LIBSTROPHE_LIBRARY} ${LIBCONFIG_LIBRARY} ${GIO_LIBRARY} ${MM-GLIB_LIBRARY} ${NETTLE_LIBRARY})

TARGET_LINK_LIBRARIES(agh ${LIBS} ${GLIB_LDFLAGS} ${LIBSTROPHE_LDFLAGS} ${LIBCONFIG_LDFLAGS} ${GIO_LDFLAGS} ${MM-GLIB_LDFLAGS} ${NETTLE_LDFLAGS})

TARGET_INCLUDE_DIRECTORIES(agh PRIVATE ${GLIB_INCLUDE_DIRS} ${LIBSTROPHE_INCLUDE_DIRS} ${LIBCONFIG_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS} ${MM-GLIB_INCLUDE_DIRS} ${NETTLE_INCLUDE_DIRS})

INSTALL(TARGETS agh
	RUNTIME DESTINATION usr/bin
)
