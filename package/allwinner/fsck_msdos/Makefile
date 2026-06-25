include $(TOPDIR)/rules.mk
include $(BUILD_DIR)/kernel.mk

# Name and release number of this package
PKG_NAME := fsck_msdos
PKG_VERSION := 0.0.1
PKG_RELEASE := 1

PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)

include $(BUILD_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION := utils
  CATEGORY := Allwinner
  TITLE := fsck_msdos
  DEPENDS :=+libstdcpp
endef

define Package/$(PKG_NAME)/description
Use to repair tfcard fat
endef

define Build/Prepare
	@$(INSTALL_DIR) $(PKG_BUILD_DIR)/
	@$(CP) ./src $(PKG_BUILD_DIR)
endef

define Build/Compile
	@$(MAKE) -C $(PKG_BUILD_DIR)/src \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		BUILD_DIR="$(PKG_BUILD_DIR)" \
		PREFIX="$(PREFIX)" \
		all
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/fsck_msdos $(1)/usr/bin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
