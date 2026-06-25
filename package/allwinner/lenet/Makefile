include $(TOPDIR)/rules.mk

PKG_NAME:=lenet
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)

include $(BUILD_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=lenet demo
  DEPENDS:=viplite-driver
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./src/* $(PKG_BUILD_DIR)/
endef

TARGET_LDFLAGS+=-L$(STAGING_DIR)/usr/lib/
TARGET_LDFLAGS+=-L$(PKG_BUILD_DIR)/sdk/drivers -Wl,-rpath-link,$(PKG_BUILD_DIR)/sdk/drivers
EXTRA_CFLAGS=-I$(PKG_BUILD_DIR)/sdk/include

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/ \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS) $(EXTRA_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		TARGET="$(PKG_NAME)"
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/$(PKG_NAME) 	$(1)/usr/bin/
	$(INSTALL_DIR) $(1)/etc/models
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/network_binary.nb $(1)/etc/models/network_binary.nb
	$(INSTALL_DIR) $(1)/etc/data
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/input_0.dat $(1)/etc/data/input_0.dat
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
