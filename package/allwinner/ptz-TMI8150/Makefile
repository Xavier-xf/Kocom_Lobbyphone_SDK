include $(TOPDIR)/rules.mk

PKG_NAME	:=ptz-TMI8150
PKG_VERSION	:=0.0.1
PKG_RELEASE	:=1

include $(BUILD_DIR)/package.mk

define Package/$(PKG_NAME)/Default
	SECTION	:=utils
	CATEGORY:=Allwinner
	TITLE	:=TMI8150 ptz drivet
endef

define Package/$(PKG_NAME)
  $(call Package/$(PKG_NAME)/Default)
  MENU:=1
  DEFAULT:=1
endef

define Package/$(PKG_NAME)/description
	TMI8150 PTZ driver
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./src $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	#libpeq.so
	$(MAKE) -C $(PKG_BUILD_DIR)/src \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CONFIG_PREFIX="$(PKG_INSTALL_DIR)" \
		all

endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/bin/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_DIR) $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/ptz $(1)/usr/bin/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
