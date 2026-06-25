#
# Copyright (C) 2006-2015 OpenWrt.org
#
# Pixman is free and open source software.
# It is available to be redistributed and/or modified under the terms of the MIT license.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=rlottie
PKG_VERSION:=0.2
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/Samsung/rlottie/
PKG_MD5SUM:=5cf29e56fe89f07a5679127250168988

PKG_LICENSE:=MIT
PKC_LICENSE_FILES:=COPYING

PKG_FIXUP:=autoreconf
PKG_INSTALL:=1

include $(BUILD_DIR)/package.mk
include $(BUILD_DIR)/cmake.mk

define Package/rlottie
  SECTION:=libs
  SUBMENU:=Libs
  CATEGORY:=Gui
  TITLE:=rlottie
  URL:=https://github.com/Samsung/rlottie/
endef

define Package/rlottie/description
  rlottie is a platform independent standalone c++ library for rendering vector
based animations and art in realtime.

Lottie loads and renders animations and vectors exported in the bodymovin JSON format.
Bodymovin JSON can be created and exported from After Effects with bodymovin, Sketch with
Lottie Sketch Export, and from Haiku.

For the first time, designers can create and ship beautiful animations without an engineer
painstakingly recreating it by hand. Since the animation is backed by JSON they are extremely
small in size but can be large in complexity!
endef

TARGET_LDFLAGS += -static-libstdc++ -ldl
TARGET_CFLAGS += $(FPIC)

define Build/InstallDev
	$(INSTALL_DIR) $(1)
	$(CP) $(PKG_INSTALL_DIR)/* $(1)/
endef

define Package/rlottie/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/*.so* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,rlottie))
