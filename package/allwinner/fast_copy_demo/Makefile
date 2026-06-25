include $(TOPDIR)/rules.mk

PKG_NAME:=fast_copy_demo
PKG_VERSION:=0.0.1
PKG_RELEASE:=1

include $(BUILD_DIR)/package.mk

define Package/$(PKG_NAME)/Default
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=Tina $(PKG_NAME)
  DEPENDS:=
endef

define Package/$(PKG_NAME)
  $(call Package/$(PKG_NAME)/Default)
  DEFAULT:=1
endef

define Package/$(PKG_NAME)/description
	Tina $(PKG_NAME)
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -arp ./src $(PKG_BUILD_DIR)
endef

define Build/Compile/$(PKG_NAME)
	$(MAKE) -C $(PKG_BUILD_DIR)/src/ \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CONFIG_PREFIX="$(PKG_INSTALL_DIR)" \
		all
endef

define Build/Compile
	$(Build/Compile/$(PKG_NAME))
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/src/lib/static/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/etc
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/fast_copy_demo $(1)/usr/bin/
	-$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/lib/dynamic/*.so $(1)/usr/lib/
	-$(INSTALL_DATA) $(PKG_BUILD_DIR)/src/file/* $(1)/etc/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))