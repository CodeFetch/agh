# This is an OpenWrt package Makefile; to build AGH use CMake instead (see the README file).
# Copyright (C) 2019 Enrico Mioso <mrkiko.rs@gmail.com>
#

include $(TOPDIR)/rules.mk

PKG_NAME:=agh
PKG_VERSION:=0.1
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-or-later
PKG_CONFIG_DEPENDS:= CONFIG_LIBNETTLE_MINI

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/agh
  SECTION:=net
  CATEGORY:=Network
  DEPENDS:=+libuci +libubus +ubusd +libubox +ubox +modemmanager +libstrophe +libnettle +libconfig +libblobmsg-json
  TITLE:=AGH XMPP control agent
  MAINTAINER:=Enrico Mioso <mrkiko.rs@gmail.com>
  PROVIDES:=agh
endef

define Package/agh/description
 If you ever wanted to control your device via XMPP, this is the right tool for you.
 Supports handling of cellular modems via ModemManager, intercepting ubus events, and system log messages when using the logd daemon from the ubox package.
endef

define Build/Prepare
	$(CP) *.c *.h CMakeLists.txt $(PKG_BUILD_DIR)/
endef

define Package/agh/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/opt
	$(INSTALL_DIR) $(1)/lib/netifd/proto
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/agh $(1)/usr/bin
	$(INSTALL_BIN) agh_bearer_setup_helper.sh $(1)/opt
	$(INSTALL_BIN) agh.proto $(1)/lib/netifd/proto/agh.sh
	$(INSTALL_BIN) wwan_agh.defaults $(1)/etc/uci-defaults
endef

$(eval $(call BuildPackage,agh))
