include $(TOPDIR)/rules.mk

PKG_NAME:=resnet50-demo
PKG_VERSION:=resnet50-demo-1.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)

include $(BUILD_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=resnet50 demo used to test npu
  DEPENDS:=+libstdcpp +viplite-driver
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./vpmRun $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Build/InstallDev
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/vpmRun $(1)/usr/bin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
