include $(TOPDIR)/rules.mk

PKG_NAME:=libawnn_full
PKG_VERSION:=libawnn_full-v1
PKG_RELEASE:=1

PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)

include $(BUILD_DIR)/package.mk

ifeq ($(CONFIG_TOOLCHAIN_LIBC), "musl")
    LIBAWNN_FULL_TOOLCHAIN_LIBC := musl
else ifeq ($(CONFIG_TOOLCHAIN_LIBC), "glibc")
    LIBAWNN_FULL_TOOLCHAIN_LIBC := glibc
endif
export LIBAWNN_FULL_TOOLCHAIN_LIBC

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=$(PKG_NAME)
  #DEPENDS:=+libstdcpp
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -fr ./sdk $(PKG_BUILD_DIR)/
	$(CP) -fr ./det_demo $(PKG_BUILD_DIR)/
endef

#define Build/Compile
#	$(MAKE) -C $(PKG_BUILD_DIR)/det_demo
#endef
define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/det_demo \
		MAKE="$(MAKE)" \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)"  \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CONFIG_PREFIX="$(PKG_INSTALL_DIR)" \
		all
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/include/libawnn
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/include/awnn_det.h $(1)/usr/include/libawnn
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/include/awnn_info.h $(1)/usr/include/libawnn
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/include/awnn_lib.h $(1)/usr/include/libawnn
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/include/cross_utils.h $(1)/usr/include/libawnn
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/include/awnn.h $(1)/usr/include/libawnn
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/library/$(LIBAWNN_FULL_TOOLCHAIN_LIBC)/libawnn_full.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/library/$(LIBAWNN_FULL_TOOLCHAIN_LIBC)/libawnn_full.a $(1)/usr/lib/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/library/$(LIBAWNN_FULL_TOOLCHAIN_LIBC)/libawnn_full.so $(1)/usr/lib/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
