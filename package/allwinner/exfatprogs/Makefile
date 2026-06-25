include $(TOPDIR)/rules.mk

PKG_NAME:=exfatprogs
PKG_VERSION:=1.2.2
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/$(PKG_NAME)/$(PKG_NAME)/tar.gz/$(PKG_VERSION)?
PKG_HASH:=3f755d35785a74138348b3a22dfcda5afc8a69e66a6a0c79be48225e6ca321ce

PKG_MAINTAINER:=Daniel Golle <daniel@makrotopia.org>
PKG_LICENSE:=GPL-2.0-only
PKG_LICENSE_FILES:=COPYING

PKG_FIXUP:=autoreconf
PKG_INSTALL:=1
PKG_BUILD_PARALLEL:=1

include $(BUILD_DIR)/package.mk

define Package/exfatprogs/Default
  SECTION:=utils
  CATEGORY:=Utilities
  SUBMENU:=Filesystem
  TITLE := exfatprogs
  DEPENDS :=+libstdcpp
endef

define Build/Prepare
	@$(INSTALL_DIR) $(PKG_BUILD_DIR)/
	@$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/exfat-mkfs
    $(Package/exfatprogs/Default)
    TITLE:=Utility for creating an exFAT File System
endef

define Package/exfat-fsck
    $(Package/exfatprogs/Default)
    TITLE:=Utility for checking/repairing an exFAT File System
endef

define Package/exfat2img
    $(Package/exfatprogs/Default)
    TITLE:=Utility for dump metadata of exFAT File System
endef

define Package/exfat-dump
    $(Package/exfatprogs/Default)
    TITLE:=Utility for dump details about exFAT File System
endef

define Package/exfat-tune
    $(Package/exfatprogs/Default)
    TITLE:=Utility for adjust tunable ondisk parameters for exFAT
endef

CONFIGURE_ARGS += \
	--enable-shared \
	--disable-static

define Package/exfat2img/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/exfat2img/exfat2img $(1)/usr/bin
endef
define Package/exfat-dump/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dump/dump.exfat $(1)/usr/bin
endef
define Package/exfat-tune/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tune/tune.exfat $(1)/usr/bin
endef
define Package/exfat-mkfs/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mkfs/mkfs.exfat $(1)/usr/bin
endef
define Package/exfat-fsck/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/fsck/fsck.exfat $(1)/usr/bin
endef

$(eval $(call BuildPackage,exfat-mkfs))
$(eval $(call BuildPackage,exfat-fsck))
$(eval $(call BuildPackage,exfat2img))
$(eval $(call BuildPackage,exfat-dump))
$(eval $(call BuildPackage,exfat-tune))
