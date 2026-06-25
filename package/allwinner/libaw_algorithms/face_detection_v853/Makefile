include $(TOPDIR)/rules.mk

PKG_NAME:=face_detect
PKG_VERSION:=5.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)


include $(BUILD_DIR)/package.mk


define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=face detect algorithm
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)/lib/
	$(CP) -r ./include $(PKG_BUILD_DIR)/
	$(CP) -r ./lib/* $(PKG_BUILD_DIR)/lib/
endef

TARGET_LDFLAGS+=-L$(PKG_BUILD_DIR)/lib/

define Build/Compile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/include/$(PKG_NAME)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/*.a $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/*.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/include/*.h $(1)/usr/include/$(PKG_NAME)
endef

define Package/$(PKG_NAME)/install
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
