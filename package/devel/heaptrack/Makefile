#
# Copyright (C) 2006-2013 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=heaptrack
PKG_VERSION:=1.4.0
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).zip
PKG_SOURCE_URL:=https://github.com/KDE/heaptrack/tree/v1.4.0

include $(BUILD_DIR)/package.mk
include $(BUILD_DIR)/cmake.mk

define Package/$(PKG_NAME)
  SECTION:=devel
  CATEGORY:=Development
  DEPENDS:=+libpthread +librt +libunwind +libstdcpp +boost-filesystem +boost-iostreams +boost-program_options +boost-system +zlib
  TITLE:=A heap memory profiler for Linux
  URL:=https://invent.kde.org/sdk/heaptrack
endef

define Package/valgrind/description
	a heap memory profiler for Linux.
endef

CMAKE_OPTIONS += -DHEAPTRACK_BUILD_PRINT=ON
PKG_JOBS = -j4

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/* $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
